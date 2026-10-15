# 3-LED Controller (XIAO ESP32-C6)

WiFi-controlled WS2813 LEDs with battery monitoring.
See [USAGE.md](USAGE.md), [WIRING.md](WIRING.md) and [DEVELOPMENT.md](DEVELOPMENT.md).

## Connect to the device

For first-time setup (or after erasing the device), connect your phone or computer
to the Wi-Fi network **leds-setup**. It is an open network and does not need a
password. If prompted that it has no internet access, choose to stay connected;
you may need to temporarily disable mobile data or automatic network switching.
Then open **http://192.168.4.1** in a browser to enter your home Wi-Fi name and
password. The device reboots after saving.

Once configured, connect your phone or computer to the same Wi-Fi network as the
device. Open the device's IP address in a browser; the serial monitor prints it
at startup as `Ready! Open http://<device-ip>`. This page is the LED control
website.

If **leds-setup** is not listed, restart the device; it only appears when Wi-Fi
has not been configured. If the setup page does not load, confirm you are still
connected to **leds-setup** and enter `http://192.168.4.1` directly. If the
control website is unreachable, check that your device and browser are on the
same Wi-Fi network and use the current IP shown in the serial monitor.

### Break from monitoring (make flash-monitor):

CTRL-T for menu
CTRL-X for exit
