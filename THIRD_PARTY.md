# Dependency and research-material policy

Adaptive-LoRa keeps dependency implementations outside the tracked project. PlatformIO downloads SDKs and RadioLib into ignored build/cache directories; no vendored SDK or full-text research paper is included.

| Dependency | Recorded version | License/source |
|---|---|---|
| RadioLib | 7.8.1, pinned | MIT, [upstream](https://github.com/jgromes/RadioLib/tree/7.8.1) |
| Arduino-ESP32 | 3.3.11, resolved baseline | LGPL-2.1, [upstream](https://github.com/espressif/arduino-esp32) |
| Unity test framework | 2.6.1, resolved baseline | MIT, [upstream](https://github.com/ThrowTheSwitch/Unity) |

Preserve upstream notices if dependency code is redistributed later. A build log mentioning a third-party component is not an included copy of that component. Cite papers using bibliographic metadata, DOI links and original research notes; distribute full text only when its license permits it.

No blanket project redistribution license was selected during repository establishment. Public hosting alone does not grant additional rights to third-party material or establish authorship/ownership of external work. License selection for project-authored work remains a maintainer decision.
