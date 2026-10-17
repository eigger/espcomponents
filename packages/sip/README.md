# SIP intercoms

Both presets use [sip_client](../../components/sip_client) and require a SIP PBX
account, networking, and `sip_password` in `secrets.yaml`.

| Device | Audio / controls |
| --- | --- |
| [Home Assistant Voice PE](voice_pe) | Voice assistant plus SIP; device controls and requirements in its README |
| [M5Stack Atom Echo](atom_echo) | Half-duplex: listen by default, hold the button to talk |

Use one preset per device. Each README includes a complete package list,
substitution defaults, secrets, and button behavior.

[All packages](../README.md)
