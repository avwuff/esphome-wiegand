# esphome-wiegand
Simple ESPHome Wiegand custom component

Based on this code:
https://github.com/Luisiado/wiegand_esphome_module

To use:
1. Drop wiegand_device.h into your esphome/ directory
2. Use the sample .yaml to create your module
3. Connect your Wiegand reader to pins D1 and D2 of your ESP8266.  These can be changed in the Yaml.

Scanned tags appear as 'tags' in Home Assistant, allowing you to make additional automations.
