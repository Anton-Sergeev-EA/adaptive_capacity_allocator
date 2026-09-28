// SPDX-License-Identifier: MIT
#include "i18n.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace acalloc::i18n {
namespace {

constexpr std::size_t kLangCount = kLanguages.size();
using row = std::array<std::string_view, kLangCount>;

// Column order follows kLanguages: ru, en, zh, hi, es, fr, de, it.
constexpr std::array<row, static_cast<std::size_t>(msg::count_)> kTable{{
    // title
    {"Адаптивный распределитель ёмкости",
     "Adaptive Capacity Allocator",
     "自适应容量分配器",
     "अनुकूली क्षमता आवंटक",
     "Asignador de capacidad adaptativo",
     "Allocateur de capacité adaptatif",
     "Adaptiver Kapazitätsallokator",
     "Allocatore di capacità adattivo"},
    // tagline
    {"Контейнер C++, который сам выбирает, насколько расти, по реальной скорости вставки.",
     "A C++ container that picks its growth factor from the measured insertion rate.",
     "一个根据实测插入速率自动选择增长因子的 C++ 容器。",
     "एक C++ कंटेनर जो मापी गई इंसर्शन दर के आधार पर अपना वृद्धि गुणांक स्वयं चुनता है।",
     "Un contenedor C++ que elige su factor de crecimiento según la velocidad de inserción medida.",
     "Un conteneur C++ qui choisit son facteur de croissance d'après le débit d'insertion mesuré.",
     "Ein C++-Container, der seinen Wachstumsfaktor anhand der gemessenen Einfügerate wählt.",
     "Un contenitore C++ che sceglie il fattore di crescita in base alla velocità di inserimento misurata."},
    // help
    {"Использование: {0} [команда] [параметры]\n\n"
     "Команды:\n"
     "  demo         наглядная демонстрация на четырёх сценариях (по умолчанию)\n"
     "  bench        сравнение скорости и памяти с std::vector\n"
     "  languages    список языков интерфейса\n\n"
     "Параметры:\n"
     "  --lang <код>      язык интерфейса: ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    число элементов в непрерывном потоке (по умолчанию 1000000)\n"
     "  --version         показать версию\n"
     "  --help            показать эту справку\n\n"
     "Язык также можно задать переменной окружения ACALLOC_LANG.",
     "Usage: {0} [command] [options]\n\n"
     "Commands:\n"
     "  demo         visual demonstration on four scenarios (default)\n"
     "  bench        speed and memory comparison with std::vector\n"
     "  languages    list interface languages\n\n"
     "Options:\n"
     "  --lang <code>     interface language: ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    elements in the continuous stream (default 1000000)\n"
     "  --version         show version\n"
     "  --help            show this help\n\n"
     "The language can also be set with the ACALLOC_LANG environment variable.",
     "用法：{0} [命令] [选项]\n\n"
     "命令：\n"
     "  demo         用四个场景进行直观演示（默认）\n"
     "  bench        与 std::vector 比较速度和内存\n"
     "  languages    列出界面语言\n\n"
     "选项：\n"
     "  --lang <代码>     界面语言：ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    连续插入场景的元素数量（默认 1000000）\n"
     "  --version         显示版本\n"
     "  --help            显示此帮助\n\n"
     "也可以通过环境变量 ACALLOC_LANG 设置语言。",
     "उपयोग: {0} [कमांड] [विकल्प]\n\n"
     "कमांड:\n"
     "  demo         चार परिदृश्यों पर दृश्य प्रदर्शन (डिफ़ॉल्ट)\n"
     "  bench        std::vector के साथ गति और मेमोरी की तुलना\n"
     "  languages    इंटरफ़ेस भाषाओं की सूची\n\n"
     "विकल्प:\n"
     "  --lang <कोड>      इंटरफ़ेस भाषा: ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    लगातार प्रवाह में तत्वों की संख्या (डिफ़ॉल्ट 1000000)\n"
     "  --version         संस्करण दिखाएँ\n"
     "  --help            यह सहायता दिखाएँ\n\n"
     "भाषा को ACALLOC_LANG पर्यावरण चर से भी सेट किया जा सकता है।",
     "Uso: {0} [comando] [opciones]\n\n"
     "Comandos:\n"
     "  demo         demostración visual con cuatro escenarios (predeterminado)\n"
     "  bench        comparación de velocidad y memoria con std::vector\n"
     "  languages    lista de idiomas de la interfaz\n\n"
     "Opciones:\n"
     "  --lang <código>   idioma de la interfaz: ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    elementos del flujo continuo (predeterminado 1000000)\n"
     "  --version         mostrar la versión\n"
     "  --help            mostrar esta ayuda\n\n"
     "El idioma también se puede fijar con la variable de entorno ACALLOC_LANG.",
     "Utilisation : {0} [commande] [options]\n\n"
     "Commandes :\n"
     "  demo         démonstration visuelle sur quatre scénarios (par défaut)\n"
     "  bench        comparaison de vitesse et de mémoire avec std::vector\n"
     "  languages    liste des langues de l'interface\n\n"
     "Options :\n"
     "  --lang <code>     langue de l'interface : ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    éléments du flux continu (1000000 par défaut)\n"
     "  --version         afficher la version\n"
     "  --help            afficher cette aide\n\n"
     "La langue peut aussi être définie par la variable d'environnement ACALLOC_LANG.",
     "Aufruf: {0} [Befehl] [Optionen]\n\n"
     "Befehle:\n"
     "  demo         anschauliche Demonstration mit vier Szenarien (Standard)\n"
     "  bench        Geschwindigkeits- und Speichervergleich mit std::vector\n"
     "  languages    Sprachen der Oberfläche auflisten\n\n"
     "Optionen:\n"
     "  --lang <Code>     Sprache der Oberfläche: ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    Elemente im kontinuierlichen Strom (Standard 1000000)\n"
     "  --version         Version anzeigen\n"
     "  --help            diese Hilfe anzeigen\n\n"
     "Die Sprache lässt sich auch über die Umgebungsvariable ACALLOC_LANG festlegen.",
     "Uso: {0} [comando] [opzioni]\n\n"
     "Comandi:\n"
     "  demo         dimostrazione visiva su quattro scenari (predefinito)\n"
     "  bench        confronto di velocità e memoria con std::vector\n"
     "  languages    elenco delle lingue dell'interfaccia\n\n"
     "Opzioni:\n"
     "  --lang <codice>   lingua dell'interfaccia: ru, en, zh, hi, es, fr, de, it\n"
     "  --elements <N>    elementi nel flusso continuo (predefinito 1000000)\n"
     "  --version         mostra la versione\n"
     "  --help            mostra questo aiuto\n\n"
     "La lingua si può impostare anche con la variabile d'ambiente ACALLOC_LANG."},
    // version_line
    {"{0}, версия {1}",
     "{0}, version {1}",
     "{0}，版本 {1}",
     "{0}, संस्करण {1}",
     "{0}, versión {1}",
     "{0}, version {1}",
     "{0}, Version {1}",
     "{0}, versione {1}"},
    // err_unknown_option
    {"Неизвестный параметр: {0}",
     "Unknown option: {0}",
     "未知选项：{0}",
     "अज्ञात विकल्प: {0}",
     "Opción desconocida: {0}",
     "Option inconnue : {0}",
     "Unbekannte Option: {0}",
     "Opzione sconosciuta: {0}"},
    // err_bad_language
    {"Язык «{0}» не поддерживается. Доступны: {1}",
     "Language \"{0}\" is not supported. Available: {1}",
     "不支持语言“{0}”。可用语言：{1}",
     "भाषा \"{0}\" समर्थित नहीं है। उपलब्ध: {1}",
     "El idioma «{0}» no está disponible. Disponibles: {1}",
     "La langue « {0} » n'est pas prise en charge. Disponibles : {1}",
     "Die Sprache „{0}“ wird nicht unterstützt. Verfügbar: {1}",
     "La lingua «{0}» non è supportata. Disponibili: {1}"},
    // err_bad_number
    {"После {0} ожидается положительное целое число",
     "{0} expects a positive integer",
     "{0} 后面需要一个正整数",
     "{0} के बाद एक धनात्मक पूर्णांक अपेक्षित है",
     "{0} requiere un número entero positivo",
     "{0} attend un entier positif",
     "{0} erwartet eine positive ganze Zahl",
     "{0} richiede un numero intero positivo"},
    // hint_help
    {"Подробнее: {0} --help",
     "See: {0} --help",
     "详见：{0} --help",
     "अधिक जानकारी: {0} --help",
     "Más información: {0} --help",
     "Plus d'informations : {0} --help",
     "Mehr dazu: {0} --help",
     "Maggiori informazioni: {0} --help"},
    // languages_header
    {"Языки интерфейса (текущий отмечен *):",
     "Interface languages (current one marked with *):",
     "界面语言（当前语言以 * 标记）：",
     "इंटरफ़ेस भाषाएँ (वर्तमान भाषा * से चिह्नित):",
     "Idiomas de la interfaz (el actual lleva *):",
     "Langues de l'interface (la langue actuelle est marquée *) :",
     "Sprachen der Oberfläche (die aktuelle ist mit * markiert):",
     "Lingue dell'interfaccia (quella attuale è segnata con *):"},
    // languages_hint
    {"Сменить язык: --lang <код> или ACALLOC_LANG=<код>",
     "Switch language: --lang <code> or ACALLOC_LANG=<code>",
     "切换语言：--lang <代码> 或 ACALLOC_LANG=<代码>",
     "भाषा बदलें: --lang <कोड> या ACALLOC_LANG=<कोड>",
     "Cambiar de idioma: --lang <código> o ACALLOC_LANG=<código>",
     "Changer de langue : --lang <code> ou ACALLOC_LANG=<code>",
     "Sprache wechseln: --lang <Code> oder ACALLOC_LANG=<Code>",
     "Cambiare lingua: --lang <codice> oppure ACALLOC_LANG=<codice>"},
    // kv_sep
    {": ", ": ", "：", ": ", ": ", " : ", ": ", ": "},
    // s_steady
    {"Сценарий 1. Непрерывный поток: {0} элементов подряд",
     "Scenario 1. Continuous stream: {0} elements in a row",
     "场景 1：连续插入 {0} 个元素",
     "परिदृश्य 1. लगातार प्रवाह: लगातार {0} तत्व",
     "Escenario 1. Flujo continuo: {0} elementos seguidos",
     "Scénario 1. Flux continu : {0} éléments d'affilée",
     "Szenario 1. Kontinuierlicher Strom: {0} Elemente am Stück",
     "Scenario 1. Flusso continuo: {0} elementi di fila"},
    // s_burst
    {"Сценарий 2. Всплески: {0} пачек по {1} элементов с паузами",
     "Scenario 2. Bursts: {0} batches of {1} elements with pauses",
     "场景 2：突发插入，{0} 批，每批 {1} 个元素，中间有停顿",
     "परिदृश्य 2. विस्फोट: {1} तत्वों के {0} बैच, बीच में विराम के साथ",
     "Escenario 2. Ráfagas: {0} lotes de {1} elementos con pausas",
     "Scénario 2. Rafales : {0} lots de {1} éléments avec des pauses",
     "Szenario 2. Schübe: {0} Pakete zu je {1} Elementen mit Pausen",
     "Scenario 2. Raffiche: {0} lotti da {1} elementi con pause"},
    // s_trickle
    {"Сценарий 3. Медленный ручеёк: {0} элементов, по одному каждые {1} мс",
     "Scenario 3. Slow trickle: {0} elements, one every {1} ms",
     "场景 3：缓慢插入 {0} 个元素，每 {1} 毫秒一个",
     "परिदृश्य 3. धीमी धारा: {0} तत्व, हर {1} ms में एक",
     "Escenario 3. Goteo lento: {0} elementos, uno cada {1} ms",
     "Scénario 3. Filet lent : {0} éléments, un toutes les {1} ms",
     "Szenario 3. Langsames Tröpfeln: {0} Elemente, eines alle {1} ms",
     "Scenario 3. Gocciolamento lento: {0} elementi, uno ogni {1} ms"},
    // s_idle
    {"Сценарий 4. Плотный поток до полного буфера, пауза {0} мс, затем новая вставка",
     "Scenario 4. Dense stream until the buffer is full, a {0} ms pause, then one more insertion",
     "场景 4：密集插入直到缓冲区满，停顿 {0} 毫秒，然后再插入一个元素",
     "परिदृश्य 4. बफ़र भरने तक घना प्रवाह, {0} ms का विराम, फिर एक और इंसर्शन",
     "Escenario 4. Flujo denso hasta llenar el búfer, pausa de {0} ms y una inserción más",
     "Scénario 4. Flux dense jusqu'à remplir le tampon, pause de {0} ms, puis une insertion de plus",
     "Szenario 4. Dichter Strom bis der Puffer voll ist, {0} ms Pause, dann eine weitere Einfügung",
     "Scenario 4. Flusso denso fino a riempire il buffer, pausa di {0} ms, poi un altro inserimento"},
    // demo_policy_note
    {"(для наглядности адаптация включается с ёмкости {0}, простоем считается пауза дольше {1} мс)",
     "(for clarity, adaptation starts at capacity {0}; a pause longer than {1} ms counts as idle)",
     "（为便于演示：容量达到 {0} 起开始自适应，停顿超过 {1} 毫秒视为空闲）",
     "(स्पष्टता के लिए अनुकूलन क्षमता {0} से शुरू होता है; {1} ms से लंबा विराम निष्क्रियता माना जाता है)",
     "(para mayor claridad, la adaptación empieza con capacidad {0}; una pausa de más de {1} ms cuenta como "
     "inactividad)",
     "(pour plus de clarté, l'adaptation commence à la capacité {0} ; une pause de plus de {1} ms compte comme une "
     "inactivité)",
     "(zur Veranschaulichung beginnt die Anpassung ab Kapazität {0}; eine Pause über {1} ms gilt als Leerlauf)",
     "(per chiarezza l'adattamento parte dalla capacità {0}; una pausa oltre {1} ms conta come inattività)"},
    // k_time
    {"время", "time", "耗时", "समय", "tiempo", "temps", "Zeit", "tempo"},
    // k_capacity
    {"ёмкость", "capacity", "容量", "क्षमता", "capacidad", "capacité", "Kapazität", "capacità"},
    // k_spare
    {"запас", "spare", "冗余", "अतिरिक्त", "holgura", "marge", "Reserve", "margine"},
    // k_reallocs
    {"перераспределений",
     "reallocations",
     "重新分配次数",
     "पुनः आवंटन",
     "reasignaciones",
     "réallocations",
     "Umlagerungen",
     "riallocazioni"},
    // k_strategy
    {"стратегия", "strategy", "策略", "रणनीति", "estrategia", "stratégie", "Strategie", "strategia"},
    // k_rate
    {"скорость", "rate", "速率", "दर", "velocidad", "débit", "Rate", "velocità"},
    // strat_aggressive
    {"агрессивная ×2,0",
     "aggressive ×2.0",
     "激进 ×2.0",
     "आक्रामक ×2.0",
     "agresiva ×2,0",
     "agressive ×2,0",
     "aggressiv ×2,0",
     "aggressiva ×2,0"},
    // strat_moderate
    {"умеренная ×1,5",
     "moderate ×1.5",
     "适中 ×1.5",
     "संतुलित ×1.5",
     "moderada ×1,5",
     "modérée ×1,5",
     "moderat ×1,5",
     "moderata ×1,5"},
    // strat_conservative
    {"консервативная ×1,1",
     "conservative ×1.1",
     "保守 ×1.1",
     "सतर्क ×1.1",
     "conservadora ×1,1",
     "conservatrice ×1,1",
     "konservativ ×1,1",
     "conservativa ×1,1"},
    // v_avg_spare
    {"→ Средний запас памяти за время заполнения: {0} у adaptive_vector против {1} у std::vector",
     "→ Average spare memory while filling: {0} for adaptive_vector vs {1} for std::vector",
     "→ 填充过程中的平均冗余内存：adaptive_vector 为 {0}，std::vector 为 {1}",
     "→ भरने के दौरान औसत अतिरिक्त मेमोरी: adaptive_vector में {0}, std::vector में {1}",
     "→ Holgura media de memoria durante el llenado: {0} en adaptive_vector frente a {1} en std::vector",
     "→ Marge mémoire moyenne pendant le remplissage : {0} pour adaptive_vector contre {1} pour std::vector",
     "→ Durchschnittliche Speicherreserve beim Füllen: {0} bei adaptive_vector gegenüber {1} bei std::vector",
     "→ Margine medio di memoria durante il riempimento: {0} per adaptive_vector contro {1} per std::vector"},
    // v_saved
    {"→ Сэкономлено памяти: {0} ({1} элементов)",
     "→ Memory saved: {0} ({1} elements)",
     "→ 节省内存：{0}（{1} 个元素）",
     "→ बचाई गई मेमोरी: {0} ({1} तत्व)",
     "→ Memoria ahorrada: {0} ({1} elementos)",
     "→ Mémoire économisée : {0} ({1} éléments)",
     "→ Eingesparter Speicher: {0} ({1} Elemente)",
     "→ Memoria risparmiata: {0} ({1} elementi)"},
    // v_more_memory
    {"→ Памяти занято больше на {0} ({1} элементов)",
     "→ Uses {0} more memory ({1} elements)",
     "→ 多占用内存：{0}（{1} 个元素）",
     "→ {0} अधिक मेमोरी ({1} तत्व)",
     "→ Usa {0} más de memoria ({1} elementos)",
     "→ Utilise {0} de mémoire en plus ({1} éléments)",
     "→ Belegt {0} mehr Speicher ({1} Elemente)",
     "→ Usa {0} di memoria in più ({1} elementi)"},
    // v_slower
    {"→ Время: в {0} раза больше, чем у std::vector",
     "→ Time: {0}× that of std::vector",
     "→ 耗时：std::vector 的 {0} 倍",
     "→ समय: std::vector का {0} गुना",
     "→ Tiempo: {0} veces el de std::vector",
     "→ Temps : {0} fois celui de std::vector",
     "→ Zeit: das {0}-Fache von std::vector",
     "→ Tempo: {0} volte quello di std::vector"},
    // v_faster
    {"→ Время: в {0} раза меньше, чем у std::vector",
     "→ Time: {0}× faster than std::vector",
     "→ 耗时：比 std::vector 快 {0} 倍",
     "→ समय: std::vector से {0} गुना तेज़",
     "→ Tiempo: {0} veces más rápido que std::vector",
     "→ Temps : {0} fois plus rapide que std::vector",
     "→ Zeit: {0}-mal schneller als std::vector",
     "→ Tempo: {0} volte più veloce di std::vector"},
    // v_same_speed
    {"→ Время: практически как у std::vector",
     "→ Time: practically the same as std::vector",
     "→ 耗时：与 std::vector 基本相同",
     "→ समय: लगभग std::vector के बराबर",
     "→ Tiempo: prácticamente igual que std::vector",
     "→ Temps : pratiquement identique à std::vector",
     "→ Zeit: praktisch wie bei std::vector",
     "→ Tempo: praticamente come std::vector"},
    // v_reallocs
    {"→ Перераспределений: {0} против {1} у std::vector",
     "→ Reallocations: {0} vs {1} for std::vector",
     "→ 重新分配：{0} 次，std::vector 为 {1} 次",
     "→ पुनः आवंटन: {0}, जबकि std::vector में {1}",
     "→ Reasignaciones: {0} frente a {1} en std::vector",
     "→ Réallocations : {0} contre {1} pour std::vector",
     "→ Umlagerungen: {0} gegenüber {1} bei std::vector",
     "→ Riallocazioni: {0} contro {1} per std::vector"},
    // v_trickle
    {"→ Скорость ≈ {0} эл./с — выбрана стратегия: {1}",
     "→ Rate ≈ {0} elements/s — strategy chosen: {1}",
     "→ 速率约 {0} 个/秒 —— 选择的策略：{1}",
     "→ दर ≈ {0} तत्व/से — चुनी गई रणनीति: {1}",
     "→ Velocidad ≈ {0} elem./s — estrategia elegida: {1}",
     "→ Débit ≈ {0} élém./s — stratégie choisie : {1}",
     "→ Rate ≈ {0} Elem./s — gewählte Strategie: {1}",
     "→ Velocità ≈ {0} elem./s — strategia scelta: {1}"},
    // v_idle
    {"→ До паузы: {0}. Сразу после паузы: {1}",
     "→ Before the pause: {0}. Right after the pause: {1}",
     "→ 停顿前：{0}。停顿后：{1}",
     "→ विराम से पहले: {0}। विराम के तुरंत बाद: {1}",
     "→ Antes de la pausa: {0}. Justo después: {1}",
     "→ Avant la pause : {0}. Juste après : {1}",
     "→ Vor der Pause: {0}. Direkt danach: {1}",
     "→ Prima della pausa: {0}. Subito dopo: {1}"},
    // summary
    {"Итог: при плотном потоке контейнер экономит память ценой скорости, при редких вставках и после "
     "простоя растёт так же быстро, как std::vector. Цифры зависят от компьютера и компилятора.",
     "Bottom line: under a dense stream the container saves memory at the cost of speed; with rare "
     "insertions and after idle periods it grows as fast as std::vector. Numbers depend on your hardware and compiler.",
     "结论：在密集插入时，容器以速度换取内存；在插入稀少或空闲之后，它像 std::vector 一样快速增长。"
     "具体数字取决于硬件和编译器。",
     "निष्कर्ष: घने प्रवाह में कंटेनर गति की कीमत पर मेमोरी बचाता है; कम इंसर्शन और निष्क्रियता के बाद यह "
     "std::vector जितनी तेज़ी से बढ़ता है। आँकड़े आपके हार्डवेयर और कंपाइलर पर निर्भर करते हैं।",
     "Conclusión: con un flujo denso el contenedor ahorra memoria a costa de velocidad; con inserciones "
     "escasas y tras la inactividad crece tan rápido como std::vector. Las cifras dependen del equipo y del "
     "compilador.",
     "En résumé : sous un flux dense, le conteneur économise la mémoire au prix de la vitesse ; avec des "
     "insertions rares et après une inactivité, il croît aussi vite que std::vector. Les chiffres dépendent "
     "du matériel et du compilateur.",
     "Fazit: Bei dichtem Strom spart der Container Speicher auf Kosten der Geschwindigkeit; bei seltenen "
     "Einfügungen und nach Leerlauf wächst er so schnell wie std::vector. Die Zahlen hängen von Hardware und Compiler "
     "ab.",
     "In sintesi: con un flusso denso il contenitore risparmia memoria a scapito della velocità; con "
     "inserimenti rari e dopo l'inattività cresce veloce quanto std::vector. I numeri dipendono da hardware e "
     "compilatore."},
    // bench_header
    {"Бенчмарк: медиана из {0} запусков, {1} элементов типа int",
     "Benchmark: median of {0} runs, {1} int elements",
     "基准测试：{0} 次运行的中位数，{1} 个 int 元素",
     "बेंचमार्क: {0} रन का माध्यिका, {1} int तत्व",
     "Benchmark: mediana de {0} ejecuciones, {1} elementos int",
     "Banc d'essai : médiane de {0} exécutions, {1} éléments int",
     "Benchmark: Median aus {0} Läufen, {1} int-Elemente",
     "Benchmark: mediana di {0} esecuzioni, {1} elementi int"},
    // bench_note
    {"Числа зависят от процессора, памяти и компилятора — запустите у себя.",
     "Numbers depend on CPU, memory and compiler — run it on your machine.",
     "数字取决于处理器、内存和编译器——请在自己的机器上运行。",
     "आँकड़े प्रोसेसर, मेमोरी और कंपाइलर पर निर्भर करते हैं — इसे अपनी मशीन पर चलाएँ।",
     "Las cifras dependen del procesador, la memoria y el compilador: ejecútelo en su equipo.",
     "Les chiffres dépendent du processeur, de la mémoire et du compilateur : lancez-le chez vous.",
     "Die Zahlen hängen von CPU, Speicher und Compiler ab – führen Sie es selbst aus.",
     "I numeri dipendono da processore, memoria e compilatore: eseguilo sulla tua macchina."},
    // unit_ms
    {"мс", "ms", "毫秒", "ms", "ms", "ms", "ms", "ms"},
    // unit_rate
    {"эл./с", "el./s", "个/秒", "तत्व/से", "elem./s", "élém./s", "Elem./s", "elem./s"},
    // unit_b
    {"Б", "B", "B", "B", "B", "o", "B", "B"},
    // unit_kb
    {"КБ", "KB", "KB", "KB", "KB", "Ko", "KB", "KB"},
    // unit_mb
    {"МБ", "MB", "MB", "MB", "MB", "Mo", "MB", "MB"},
}};

std::size_t g_lang = 0;  // index into kLanguages; 0 = Russian

std::size_t index_of(std::string_view code) noexcept {
    for (std::size_t i = 0; i < kLangCount; ++i) {
        if (kLanguages[i].code == code) return i;
    }
    return kLangCount;
}

// "de_DE.UTF-8" -> "de", "zh-Hans-CN" -> "zh", "C" / "POSIX" -> "".
std::string normalise(std::string_view raw) {
    std::string out;
    for (char c : raw) {
        if (c == '_' || c == '-' || c == '.' || c == '@' || c == ':') break;
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (out == "c" || out == "posix") out.clear();
    return out;
}

char thousands_separator() noexcept {
    switch (g_lang) {
        case 0:  // ru
        case 5:  // fr
            return ' ';
        case 4:  // es
        case 6:  // de
        case 7:  // it
            return '.';
        default:
            return ',';
    }
}

bool decimal_comma() noexcept {
    return g_lang == 0 || g_lang >= 4;
}

}  // namespace

bool is_supported(std::string_view code) noexcept {
    return index_of(code) < kLangCount;
}

bool set_language(std::string_view code) noexcept {
    const std::size_t i = index_of(code);
    if (i >= kLangCount) return false;
    g_lang = i;
    return true;
}

std::string_view current_language() noexcept {
    return kLanguages[g_lang].code;
}

std::string detect_language() {
    for (const char* var : {"ACALLOC_LANG", "LC_ALL", "LC_MESSAGES", "LANG"}) {
        if (const char* v = std::getenv(var); v != nullptr && *v != '\0') {
            const std::string code = normalise(v);
            if (is_supported(code)) return code;
            if (!code.empty()) break;  // an explicit but unsupported locale: fall back to default
        }
    }
    if (const char* v = std::getenv("LANGUAGE"); v != nullptr) {
        std::string_view list(v);
        while (!list.empty()) {
            const auto colon = list.find(':');
            const std::string code = normalise(list.substr(0, colon));
            if (is_supported(code)) return code;
            if (colon == std::string_view::npos) break;
            list.remove_prefix(colon + 1);
        }
    }
#ifdef _WIN32
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0) {
        std::string ascii;
        for (const wchar_t* p = name; *p != 0 && *p < 128; ++p) ascii.push_back(static_cast<char>(*p));
        const std::string code = normalise(ascii);
        if (is_supported(code)) return code;
    }
#endif
    return std::string(kDefaultLanguage);
}

std::string_view tr(msg id) noexcept {
    return kTable[static_cast<std::size_t>(id)][g_lang];
}

std::string tr(msg id, std::initializer_list<std::string_view> args) {
    const std::string_view text = tr(id);
    std::string out;
    out.reserve(text.size() + 32);
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{' && i + 2 < text.size() && text[i + 2] == '}' && text[i + 1] >= '0' && text[i + 1] <= '9') {
            const auto n = static_cast<std::size_t>(text[i + 1] - '0');
            if (n < args.size()) {
                out.append(*(args.begin() + n));
                i += 2;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

std::string format_int(std::uint64_t value) {
    const std::string digits = std::to_string(value);
    const char sep = thousands_separator();
    const bool indian = g_lang == 3;  // hi: 12,34,567
    std::string out;
    const std::size_t n = digits.size();
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(digits[i]);
        const std::size_t rest = n - 1 - i;  // digits remaining to the right
        if (rest == 0) break;
        const bool group = indian ? (rest == 3 || (rest > 3 && (rest - 3) % 2 == 0)) : (rest % 3 == 0);
        if (group) out.push_back(sep);
    }
    return out;
}

std::string format_fixed(double value, int decimals) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, value);
    std::string s(buf);
    if (decimal_comma()) std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

std::string format_bytes(double bytes) {
    const double a = std::fabs(bytes);
    if (a >= 1024.0 * 1024.0) return format_fixed(bytes / (1024.0 * 1024.0), 2) + " " + std::string(tr(msg::unit_mb));
    if (a >= 1024.0) return format_fixed(bytes / 1024.0, 1) + " " + std::string(tr(msg::unit_kb));
    return format_fixed(bytes, 0) + " " + std::string(tr(msg::unit_b));
}

std::string_view strategy_name(std::string_view id) noexcept {
    if (id == "conservative") return tr(msg::strat_conservative);
    if (id == "moderate") return tr(msg::strat_moderate);
    return tr(msg::strat_aggressive);
}

void prepare_console() noexcept {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

}  // namespace acalloc::i18n
