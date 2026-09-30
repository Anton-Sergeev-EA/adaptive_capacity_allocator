# अनुकूली क्षमता आवंटक

[Русский](README.md) · [English](README.en.md) · [中文](README.zh.md) · **हिन्दी** · [Español](README.es.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Italiano](README.it.md)

[![CI](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml/badge.svg)](https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator/actions/workflows/ci.yml)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Header-only](https://img.shields.io/badge/header--only-yes-success.svg)](include/adaptive/adaptive_allocator.hpp)

`adaptive_vector` एक C++17 कंटेनर है जिसका इंटरफ़ेस `std::vector` जैसा है और जो मापी गई इंसर्शन दर के आधार पर
**खुद तय करता है कि कितना बढ़ना है**। घने इंसर्शन प्रवाह में यह मेमोरी बचाता है; कम इंसर्शन पर यह साधारण
`std::vector` जितनी तेज़ी से बढ़ता है।

**[ब्राउज़र में इंटरैक्टिव डेमो →](https://anton-sergeev-ea.github.io/adaptive_capacity_allocator/)** (8 भाषाएँ)

## इसकी ज़रूरत क्यों है

जगह कम पड़ने पर `std::vector` हमेशा एक ही गुणांक से बढ़ता है: GCC और Clang में 2 गुना, MSVC में 1.5 गुना। यह तेज़ है,
लेकिन दोगुना होने के तुरंत बाद आवंटित मेमोरी का आधा हिस्सा तक खाली पड़ा रह सकता है। जो सेवाएँ मेमोरी में लाखों बफ़र
रखती हैं (संदेश कतारें, लॉग, टेलीमेट्री, ऑर्डर बुक), उनमें ये खाली हिस्से जुड़कर गीगाबाइट बन जाते हैं।

`adaptive_vector` मापता है कि तत्व कितनी तेज़ी से डाले जा रहे हैं और लोड के अनुसार वृद्धि गुणांक चुनता है:

| इंसर्शन दर | रणनीति | वृद्धि |
|---|---|:---:|
| 100/से तक, निष्क्रियता के बाद पहला इंसर्शन, 1,024 तत्वों से छोटे बफ़र | आक्रामक | ×2.0 |
| 100 से 1,000/से | संतुलित | ×1.5 |
| 1,000/से से अधिक | सतर्क | ×1.1 |

सभी सीमाएँ और गुणांक `growth_policy` के ज़रिए बदले जा सकते हैं।

## आँकड़े

Linux x86_64, GCC 13.3, `-O3`, 10,00,000 `int` इंसर्शन, 5 रन का माध्यिका (`acalloc bench`):

| | समय | भरने के दौरान औसत अतिरिक्त मेमोरी | पुनः आवंटन |
|---|:---:|:---:|:---:|
| `std::vector<int>` | ~4.4 ms | 36.4 % | 21 |
| `adaptive_vector<int>` 2.0 | ~8.8 ms | **5.0 %** | 81 |
| `adaptive_vector<int>` 1.0 | ~40 ms | 4.9 % | 129 |

**कीमत वास्तविक है।** दोगुना होने के बजाय 10% बढ़ने के लिए अधिक पुनः आवंटन और कॉपी चाहिए, इसलिए लगातार प्रवाह में
कंटेनर `std::vector` से लगभग 2 गुना धीमा है। बदले में औसत अतिरिक्त मेमोरी 35–50% के बजाय लगभग 5% रहती है। अगर आपके
काम के लिए मेमोरी थ्रूपुट से ज़्यादा महत्वपूर्ण है तो यह अच्छा सौदा है; वरना `std::vector` ही रखें। किसी एक बिंदु पर
अंतिम क्षमता इस पर निर्भर करती है कि भरना कहाँ रुका: ठीक 10,00,000 तत्वों पर `std::vector` अपने सबसे अच्छे मामले (2²⁰)
पर होता है, इसलिए निष्पक्ष तुलना पूरे भरने के दौरान औसत अतिरिक्त मेमोरी की है, न कि केवल अंतिम बिंदु की।

इसे अपनी मशीन पर मापें — परिणाम प्रोसेसर, मेमोरी और कंपाइलर पर निर्भर करता है:

```bash
./build/acalloc bench
```

## जल्दी शुरुआत

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(adaptive_allocator
    GIT_REPOSITORY https://github.com/Anton-Sergeev-EA/adaptive_capacity_allocator.git
    GIT_TAG        v2.0.0)
FetchContent_MakeAvailable(adaptive_allocator)

target_link_libraries(your_app PRIVATE adaptive::allocator)
```

या इंस्टॉल करने के बाद (`cmake --install build`):

```cmake
find_package(AdaptiveAllocator 2 REQUIRED)
target_link_libraries(your_app PRIVATE adaptive::allocator)
```

या बस [`include/adaptive/adaptive_allocator.hpp`](include/adaptive/adaptive_allocator.hpp) को अपने प्रोजेक्ट में कॉपी करें:
यह केवल मानक लाइब्रेरी पर निर्भर है।

### उपयोग

```cpp
#include <adaptive/adaptive_allocator.hpp>
#include <iostream>

int main() {
    adaptive::adaptive_vector<int> v;
    for (int i = 0; i < 1'000'000; ++i) v.push_back(i);

    const auto s = v.stats();
    std::cout << "क्षमता: " << s.capacity
              << ", पुनः आवंटन: " << s.reallocations
              << ", रणनीति: " << adaptive::to_string(s.last_strategy) << '\n';
}
```

### अपनी वृद्धि नीति

```cpp
adaptive::growth_policy p;
p.conservative_factor = 1.25;                    // 1.1 से नरम
p.high_threshold = 50'000;                        // सतर्क केवल 50,000 इंसर्शन/से से ऊपर
p.idle_reset = std::chrono::milliseconds(500);    // 0.5 सेकंड से लंबा विराम = निष्क्रियता
adaptive::adaptive_vector<Order> book(p);
```

### कई कंटेनरों के लिए एक टेलीमेट्री

```cpp
auto shared = std::make_shared<adaptive::allocation_telemetry>();
adaptive::adaptive_vector<int> a(shared), b(shared);  // संयुक्त दर पर निर्णय, बिना लॉक के
```

## 8 भाषाओं में टर्मिनल डेमो

`acalloc` प्रोग्राम चार परिदृश्यों में कंटेनर का व्यवहार दिखाता है: लगातार प्रवाह, विस्फोट, धीमी धारा और विराम वाला प्रवाह।
भाषा सिस्टम की भाषा से अपने आप पहचानी जाती है।

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/acalloc                 # प्रदर्शन
./build/acalloc bench           # std::vector से तुलना
./build/acalloc languages       # भाषाओं की सूची
./build/acalloc --lang hi       # हिन्दी इंटरफ़ेस
```

समर्थित भाषाएँ: Русский (मुख्य), English, 中文, हिन्दी, Español, Français, Deutsch, Italiano।
भाषा को `ACALLOC_LANG` पर्यावरण चर से भी सेट किया जा सकता है। नई भाषा जोड़ने के लिए
[docs/TRANSLATING.md](docs/TRANSLATING.md) देखें।

## API

| प्रकार | उद्देश्य |
|---|---|
| `adaptive_vector<T>` | `std::vector` इंटरफ़ेस वाला कंटेनर: `push_back`, `emplace_back`, `insert`, `emplace`, `erase`, `resize`, `assign`, `reserve`, इटरेटर, तुलना, `swap`, साथ ही `stats()` और `telemetry()` |
| `growth_policy` | वृद्धि गुणांक, दर सीमाएँ, माप विंडो, निष्क्रियता सीमा, अनुकूलन के लिए न्यूनतम क्षमता |
| `allocation_telemetry` | थ्रेड-सुरक्षित इंसर्शन दर ट्रैकर; कंटेनरों और थ्रेड के बीच साझा किया जा सकता है |
| `adaptive_allocator<T>` | मानक एलोकेटर जो अधिक संरेखण वाले प्रकारों (`alignas(64)` आदि) का समर्थन करता है |

अंदर की व्यवस्था:

- **कोई बैकग्राउंड थ्रेड नहीं।** संस्करण 1.0 हर वेक्टर के लिए एक थ्रेड शुरू करता था; 10,000 वेक्टर यानी 10,000 थ्रेड।
- **हर इंसर्शन पर घड़ी नहीं पढ़ी जाती।** कंटेनर इंसर्शन स्थानीय रूप से गिनता है और हर 64 इंसर्शन पर तथा हर वृद्धि निर्णय पर
  टेलीमेट्री को रिपोर्ट करता है। `push_back` का हॉट पाथ एक काउंटर और दो तुलनाएँ है।
- **निष्क्रियता पहचानी जाती है।** अगर `idle_reset` से अधिक समय तक कोई इंसर्शन नहीं हुआ, तो पुराने आँकड़े भुला दिए जाते हैं
  और विराम के बाद पहला इंसर्शन आक्रामक वृद्धि करता है।

## बिल्ड, टेस्ट और जाँच

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

सैनिटाइज़र (ASan और TSan को एक ही प्रोग्राम में नहीं जोड़ा जा सकता, इसलिए तीन में से एक चुनें):

```bash
cmake -S . -B build-asan -DADAPTIVE_SANITIZER=address -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build-tsan -DADAPTIVE_SANITIZER=thread  -DCMAKE_BUILD_TYPE=Debug
```

GCC और Clang के साथ Docker बिल्ड [docker/README.md](docker/README.md) में वर्णित है।
CI में Linux (GCC और Clang, Debug और Release, C++17 और C++20), macOS, Windows (MSVC), दोनों सैनिटाइज़र और फ़ॉर्मैटिंग की जाँच होती है।

## नया क्या है

[CHANGELOG.md](CHANGELOG.md) देखें। संस्करण 1.0 के लिए लिखा गया कोड (`#include "adaptive_allocator.hpp"`,
`allocation_telemetry(idle_ms, window_ms, threshold)` कंस्ट्रक्टर, `compute_capacity`, `record_insertion`) बिना बदलाव के
कंपाइल होता रहेगा।

## लाइसेंस

MIT — [LICENSE](LICENSE) देखें।

---

Anton Sergeev — avsergeev1981@gmail.com
