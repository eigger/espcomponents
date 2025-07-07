# 1. HW 구성
- **문서 링크**: [M5Stack Atom RS485 문서](https://docs.m5stack.com/en/atom/Atomic%20RS485%20Base)
- **이미지**:
  <br>
  <img src="https://static-cdn.m5stack.com/resource/docs/products/atom/Atomic%20RS485%20Base/img-482d74b8-0462-4139-943a-b8bf43da18c4.webp" alt="M5Stack Atom RS485" width="300">
  <img src="https://static-cdn.m5stack.com/resource/docs/products/atom/tail485/tail485_01.webp" alt="M5Stack Atom RS485 Tail" width="300">
  <br>
  <img src="https://static-cdn.m5stack.com/resource/docs/products/core/ATOM%20Lite/img-b87e8051-87c6-41d7-a710-eb27f62eb785.webp" alt="M5Stack Atom Lite" width="300">
  <img src="https://static-cdn.m5stack.com/resource/docs/products/core/AtomS3%20Lite/img-dc6432b6-fd9b-4066-9a4d-49786503d1a3.webp" alt="M5Stack Atom Lite" width="300">
  <br>
1. **구매 링크**: [알리익스프레스(RS485)](https://ko.aliexpress.com/item/1005005912210853.html?pdp_npi=4%40dis%21KRW%21%E2%82%A9%2015%2C450%21%E2%82%A9%2015%2C450%21%21%2110.19%2110.19%21%402101246417367405806363493eb3a8%2112000034821216022%21sh%21KR%210%21X&spm=a2g0o.store_pc_allItems_or_groupList.new_all_items_2007608036152.1005005912210853&gatewayAdapt=glo2kor) RX:GPIO22, TX:GPIO19<br>
or [알리익스프레스(RS485 Tail)](https://ko.aliexpress.com/item/1005003297531645.html?pdp_npi=4%40dis%21USD%21US%20%246.95%21US%20%240.99%21%21%216.95%210.99%21%40212a6e3217429992481557241e10e2%2112000025078858343%21sh%21KR%210%21X&spm=a2g0o.store_pc_allItems_or_groupList.new_all_items_2007608036152.1005003297531645&gatewayAdapt=glo2kor) RX:GPIO32, TX:GPIO26 중 택1
2. **구매 링크**: [알리익스프레스(Atom)](https://ko.aliexpress.com/item/1005003299215808.html?pdp_npi=4%40dis%21KRW%21₩%2011%2C341%21₩%2011%2C341%21%21%217.65%217.65%21%402140c1c317374652352956593e85b5%2112000025086683331%21sh%21KR%210%21X&spm=a2g0o.store_pc_allItems_or_groupList.new_all_items_2007608036152.1005003299215808&gatewayAdapt=glo2kor) or [알리익스프레스(AtomS3)](https://ko.aliexpress.com/item/1005005177952629.html?pdp_npi=4%40dis%21KRW%21₩%2011%2C400%21₩%2011%2C400%21%21%217.69%217.69%21%40210123bc17374653830282226e4c98%2112000031987959059%21sh%21KR%210%21X&spm=a2g0o.store_pc_allItems_or_groupList.new_all_items_2007608036152.1005005177952629&gatewayAdapt=glo2kor) 중 택 1

- 485Base or 485Tail + Atom(ESP32) Lite or AtomS3 Lite 구매
- 485모듈과 ESP32모듈 각 1개 (총 2개 구매)

<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/673/C008_PinMap_01.jpg" alt="M5Stack Atom Lite" width="100%">

- 485Base + Atom Lite
  ```yaml
  uart:
    rx_pin: GPIO22
    tx_pin: GPIO19
  ```
- 485Tail + Atom Lite
  ```yaml
  uart:
    rx_pin: GPIO32
    tx_pin: GPIO26
  ```
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/471/C124_PinMap_01.jpg" alt="M5Stack Atom Lite" width="100%">

- 485Base + AtomS3 Lite
  ```yaml
  uart:
    rx_pin: GPIO05
    tx_pin: GPIO06
  ```
- 485Tail + AtomS3 Lite
  ```yaml
  uart:
    rx_pin: GPIO01
    tx_pin: GPIO02
  ```

---
- **소형 디자인**
  - 컴팩트한 크기로 간결하고 일체형의 구조.
- **편리한 RS485 통신 지원**
  - 산업 표준 통신 프로토콜을 손쉽게 활용 가능.
  - TTL 신호를 RS485로 변환하는 내장 전환 칩 탑재.
- **12V DC 컨버터 내장**
  - 내장된 12V DC 컨버터를 통해 12V 환경에서도 별도의 컨버터 없이 동작 가능.
- **확장성과 호환성**
  - M5Stack 생태계와의 완벽한 호환성.
  - ESP32 기반으로 Wi-Fi 및 Bluetooth 기능 함께 지원.
- **산업용 및 상업용 응용 가능성**
  - 강력한 노이즈 내성을 가진 RS485로 다양한 산업 환경에서 사용 가능.
  - 멀티 드롭 지원으로 여러 장치 간 통신 가능.
---
