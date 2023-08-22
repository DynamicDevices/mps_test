
This example shows a simple test case for the nRF52840 Dongle which

- is based on the Nordic nRF Coap Client example with CLI
- connects to a predefined OpenThread mesh
- initiates a Bluetooth scan
- toggles the yellow LED until the firmware locks up

Build for the nrf52840dongle_nrf52840 and program with the Nordic Programming Tool

NOTE: The FTD build seems to fail The MTD build seems more reliable? Make sure to set FTD in `prj.conf`

```
CONFIG_OPENTHREAD_MTD=y
#CONFIG_OPENTHREAD_FTD=y
```

We are building against a vanilla installation of Nordic nRF Connect SDK v2.4.0

Under test this fails in a few seconds - 20 seconds

To make it fail I can also do a ping e.g. to an OTBR address or Google DNS

```
ot ping 8.8.8.8
```
