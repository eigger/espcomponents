"""Warnings for packet definitions that cannot match received UARTEx data."""

import logging

from .const import CONF_RX_CHECKSUM, CONF_RX_CHECKSUM_2

_LOGGER = logging.getLogger(__name__)


def _masked_bytes(data, mask, label):
    for index, (expected, bits) in enumerate(zip(data, mask)):
        if expected & ~bits:
            _LOGGER.warning(
                "UARTEx %s byte %d expects 0x%02X, but mask 0x%02X clears expected bits; it cannot match",
                label, index, expected, bits,
            )


def _possible_mask(data, mask):
    return all(not (expected & ~bits) for expected, bits in zip(data, mask))


def _shadows(earlier, later):
    """Whether every later-header match also matches the earlier prefix."""
    if len(earlier["data"]) > len(later["data"]):
        return False
    earlier_mask = earlier.get("mask", [])
    later_mask = later.get("mask", [])
    for i, expected in enumerate(earlier["data"]):
        required = earlier_mask[i] if i < len(earlier_mask) else 0xFF
        supplied = later_mask[i] if i < len(later_mask) else 0xFF
        if required & ~supplied or (later["data"][i] & required) != expected:
            return False
    return True


def _payload_lengths(hub):
    length = hub.get("rx_length")
    if not isinstance(length, int):
        return None
    headers = [header for header in hub.get("rx_header", []) if header["data"]]
    if not headers:
        headers = [{"data": []}]
    checksum = 0
    for key, width in ((CONF_RX_CHECKSUM, 1), (CONF_RX_CHECKSUM_2, 2)):
        if key in hub and hub[key] != "NONE":
            checksum = width
    trailer = len(hub.get("rx_footer", [])) + checksum
    viable = [header for header in headers
              if _possible_mask(header["data"], header.get("mask", []))
              and length >= len(header["data"]) + trailer + 1]
    selected = []
    for header in viable:
        if not any(_shadows(earlier, header) for earlier in selected):
            selected.append(header)
    return [length - len(header["data"]) - trailer for header in selected]


def _state(state, label, lengths):
    if not isinstance(state, dict):
        return
    if "data" in state:
        data = state["data"]
        inverted = state.get("inverted", False)
        exact = state.get("match") == "EXACT"
        if not inverted:
            _masked_bytes(data, state.get("mask", []), label)
        if inverted and not exact:
            return
        needed = state.get("offset", 0) + len(data)
        fits = lambda size: size == needed if exact else size >= needed
        if lengths and not any(fits(size) for size in lengths):
            _LOGGER.warning("UARTEx %s needs %s payload length %d, but fixed-frame payload lengths are %s; check offset, length and framing",
                            label, "exact" if exact else "at least", needed, lengths)
    elif "offset" in state and "length" in state:
        offset, needed = state["offset"], state["offset"] + state["length"]
        if lengths and not any(size >= needed for size in lengths):
            effect = "entirely missing" if all(size <= offset for size in lengths) else "truncated"
            _LOGGER.warning("UARTEx %s numeric field will be %s: it ends at payload offset %d, but fixed-frame payload lengths are %s; check offset, length and framing",
                            label, effect, needed, lengths)


def _debounce(debounce, label, lengths):
    if not isinstance(debounce, dict):
        return
    mask = debounce.get("mask", [])
    delay = debounce.get("delay")
    if not mask or not lengths or not getattr(delay, "total_milliseconds", 0):
        return
    offset = debounce.get("offset", 0)
    needed = offset + len(mask)
    if not any(size >= needed for size in lengths):
        if all(size <= offset for size in lengths):
            effect = "entirely outside the payload: all packets would compare equal and debounce would be ineffective"
        else:
            effect = "partly outside the payload: those bytes count as equal"
        _LOGGER.warning("UARTEx %s mask is %s; it ends at payload offset %d, but fixed-frame payload lengths are %s; check offset, mask and framing",
                        label, effect, needed, lengths)


def _command(command, label, lengths):
    if not isinstance(command, dict):
        return
    ack = command.get("ack", [])
    if not ack:
        return
    _masked_bytes(ack, command.get("mask", []), label + ".ack")
    if lengths is not None and lengths and not any(size >= len(ack) for size in lengths):
        _LOGGER.warning("UARTEx %s.ack needs at least %d payload bytes, but fixed-frame payload lengths are %s; check ACK and framing",
                        label, len(ack), lengths)


def validate_packets(hub, config):
    """Inspect normalized definitions attached to one UARTEx hub."""
    hub_id = str(hub["id"])
    lengths = _payload_lengths(hub)
    if lengths == []:
        _LOGGER.warning("UARTEx %s fixed receive frame cannot fit a viable header, at least one payload byte, and configured trailer; check rx_length and framing", hub_id)
    for i, header in enumerate(hub.get("rx_header", [])):
        _masked_bytes(header["data"], header.get("mask", []), f"{hub_id}.rx_header[{i}]")
    for i, reply in enumerate(hub.get("rx_reply", [])):
        _state(reply["state"], f"{hub_id}.rx_reply[{i}].state", lengths)
        _command(reply["command"], f"{hub_id}.rx_reply[{i}].command", lengths)
    for domain, entries in config.items():
        if not isinstance(entries, list):
            continue
        for entity in entries:
            if not isinstance(entity, dict) or str(entity.get("uartex_id")) != hub_id:
                continue
            name = entity.get("name") or str(entity.get("id", domain))
            for key, value in entity.items():
                label = f"{hub_id}.{name}.{key}"
                if key == "state_debounce":
                    _debounce(value, label, lengths)
                elif key == "state" or key.startswith("state_"):
                    _state(value, label, lengths)
                elif key == "command" or key.startswith("command_"):
                    _command(value, label, lengths)
