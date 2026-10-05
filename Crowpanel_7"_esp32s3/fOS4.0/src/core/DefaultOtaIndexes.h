#pragma once

#include <Arduino.h>

namespace FOSDefaultIndexes {

static const char kOta[] PROGMEM = R"FOSIDX(FOS_OTA_INDEX_V1
fOS4.0-rc.1.ino.bin	2840320
fOS3.3.0.ino.bin	1886912
fOS3.2.0.ino.bin	1877552
fOS3.1.0.ino.bin	1808688
fOS3.0.0.ino.bin	1804240
fOS3.0.0-beta.1.ino.bin	1802192
fOS2.5.0.ino.bin	1788816
fOS2.4.0.ino.bin	1786096
fOS2.3.0.ino.bin	1782816
fOS2.2.1.ino.bin	1771216
fOS2.2.0.ino.bin	1771216
fOS1.5.0.ino.bin	1993440
)FOSIDX";

static const char kRecovery[] PROGMEM = R"FOSIDX(FOS_OTA_INDEX_V1
recovery.ino.bin	436784
)FOSIDX";

}  // namespace FOSDefaultIndexes
