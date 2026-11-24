# Dependency and research-material policy

Adaptive-LoRa keeps dependency implementations outside the tracked project. PlatformIO downloads SDKs and RadioLib into ignored build/cache directories; no vendored SDK or full-text research paper is included.

| Dependency | Recorded version | License/source |
|---|---|---|
| RadioLib | 7.8.1, pinned | MIT, [upstream](https://github.com/jgromes/RadioLib/tree/7.8.1) |
| Arduino-ESP32 | 3.3.11, resolved baseline | LGPL-2.1, [upstream](https://github.com/espressif/arduino-esp32) |
| Unity test framework | 2.6.1, resolved baseline | MIT, [upstream](https://github.com/ThrowTheSwitch/Unity) |

Preserve upstream notices if dependency code is redistributed later. A build log mentioning a third-party component is not an included copy of that component. Cite papers using bibliographic metadata, DOI links and original research notes; distribute full text only when its license permits it.

Project-authored code is licensed under [GNU GPL version 3 only](LICENSE). Third-party code, cited papers, attributed figures and product marks retain their own terms. The two README SVG additions are original project artwork. Check contributor permissions and any material authored under an employer or other agreement before making a formal distribution. The repository history alone does not establish those rights.

Internal use does not by itself require public release. If you convey a firmware binary, check GPLv3 Corresponding Source and linked-component obligations for that distribution. Include the applicable build instructions and upstream notices. The table records direct dependencies. Review the actual linked SDK components and their terms before publishing a binary.
