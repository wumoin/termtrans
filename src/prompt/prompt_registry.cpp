/**
 * @file prompt_registry.cpp
 * @brief 实现目标语言 prompt 注册表。
 */

#include "prompt/prompt_registry.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace termtrans::prompt {
namespace {

/**
 * @brief 内置目标语言定义。
 *
 * 每个 prompt 都是使用目标语言书写的完整文本。这里不使用目标语言名称
 * 占位符动态拼接，避免后续维护者误把 prompt 表退化成通用模板。
 */
struct BuiltinPrompt {
  std::string_view code;  // 内置目标语言代码。
  std::string_view display_name;  // 内置展示名称。
  std::string_view prompt;  // 完整独立 prompt 文本。
};

constexpr std::array<BuiltinPrompt, 25> kBuiltinPrompts = {{
    {"ar",
     "العربية",
     "ترجم النص المدخل إلى العربية.\n"
     "أخرج النص المترجم فقط؛ لا تشرح، ولا تلخص، ولا تجب عن الأسئلة، ولا تضف "
     "ملاحظات.\n"
     "حافظ على ترتيب الفقرات والأسطر الفارغة وفواصل الأسطر الواضحة والقوائم "
     "والترقيم والمسافات البادئة والجداول وتخطيط الطرفية.\n"
     "حافظ على كتل الشيفرة والأوامر وأسماء الخيارات ومتغيرات البيئة والمسارات "
     "وعناوين URL والعناصر النائبة وأسماء المتغيرات والوسوم وبنية Markdown.\n"
     "ترجم اللغة الطبيعية فقط؛ اترك الشيفرة والإعدادات والسجلات ومخرجات "
     "الأوامر كما هي عندما يضر ترجمتها بالمعنى.\n"
     "اجعل النص العادي طبيعيًا، لكن لا تدمج الفقرات أو تعيد ترتيب المحتوى أو "
     "تغير الإخراج إلى تنسيق جديد."},
    {"bn",
     "বাংলা",
     "ইনপুট পাঠ্যটি বাংলায় অনুবাদ করুন।\n"
     "শুধু অনুবাদিত পাঠ্য আউটপুট করুন; ব্যাখ্যা, সারাংশ, প্রশ্নের উত্তর বা "
     "অতিরিক্ত মন্তব্য যোগ করবেন না।\n"
     "মূল অনুচ্ছেদের ক্রম, ফাঁকা লাইন, স্পষ্ট লাইন ব্রেক, তালিকা, নম্বরিং, "
     "ইন্ডেন্টেশন, টেবিল এবং টার্মিনাল লেআউট সংরক্ষণ করুন।\n"
     "কোড ব্লক, কমান্ড, অপশনের নাম, environment variable, পাথ, URL, "
     "প্লেসহোল্ডার, ভেরিয়েবলের নাম, ট্যাগ এবং Markdown কাঠামো সংরক্ষণ করুন।\n"
     "শুধু প্রাকৃতিক ভাষা অনুবাদ করুন; কোড, কনফিগ, লগ এবং কমান্ড আউটপুট "
     "অনুবাদ করলে অর্থ নষ্ট হলে সেগুলো অপরিবর্তিত রাখুন।\n"
     "সাধারণ গদ্য স্বাভাবিকভাবে অনুবাদ করুন, কিন্তু অনুচ্ছেদ মেলাবেন না, "
     "বিষয়বস্তু পুনর্বিন্যাস করবেন না বা আউটপুটকে নতুন ফরম্যাটে বদলাবেন না।"},
    {"de",
     "Deutsch",
     "Übersetze den Eingabetext ins Deutsche.\n"
     "Gib nur den übersetzten Text aus. Erkläre nichts, fasse nichts zusammen, "
     "beantworte keine Fragen und füge keine Hinweise hinzu.\n"
     "Erhalte die ursprüngliche Absatzreihenfolge, Leerzeilen, harte "
     "Zeilenumbrüche, Listen, Nummerierungen, Einrückungen, Tabellen und das "
     "Terminal-Layout.\n"
     "Erhalte Codeblöcke, Befehle, Optionsnamen, Umgebungsvariablen, Pfade, "
     "URLs, Platzhalter, Variablennamen, Tags und die Markdown-Struktur.\n"
     "Übersetze nur natürliche Sprache; lasse Code, Konfigurationen, Logs und "
     "Befehlsausgaben unverändert, wenn eine Übersetzung ihre Bedeutung "
     "zerstören würde.\n"
     "Formuliere normalen Fließtext natürlich, aber führe keine Absätze "
     "zusammen, ordne Inhalte nicht neu und ändere die Ausgabe nicht in ein "
     "neues Format."},
    {"en",
     "English",
     "Translate the input text into English.\n"
     "Only output the translated text. Do not explain, summarize, answer "
     "questions, or add notes.\n"
     "Preserve the original paragraph order, blank lines, hard line breaks, "
     "lists, numbering, indentation, tables, and terminal layout.\n"
     "Preserve code blocks, commands, option names, environment variables, "
     "paths, URLs, placeholders, variable names, tags, and Markdown structure.\n"
     "Translate natural language only; keep code, config, logs, and command "
     "output unchanged when translating them would break meaning.\n"
     "Make ordinary prose read naturally, but do not merge paragraphs, reorder "
     "content, or change the output into a new format."},
    {"es",
     "Español",
     "Traduce el texto de entrada al español.\n"
     "Devuelve solo el texto traducido. No expliques, no resumas, no respondas "
     "preguntas ni añadas notas.\n"
     "Conserva el orden de los párrafos, las líneas en blanco, los saltos de "
     "línea explícitos, las listas, la numeración, la sangría, las tablas y el "
     "diseño de terminal.\n"
     "Conserva bloques de código, comandos, nombres de opciones, variables de "
     "entorno, rutas, URL, marcadores de posición, nombres de variables, "
     "etiquetas y la estructura Markdown.\n"
     "Traduce solo el lenguaje natural; deja sin cambios el código, la "
     "configuración, los logs y la salida de comandos cuando traducirlos "
     "rompería su significado.\n"
     "Haz que la prosa normal suene natural, pero no fusiones párrafos, no "
     "reordenes contenido ni cambies la salida a un formato nuevo."},
    {"fa",
     "فارسی",
     "متن ورودی را به فارسی ترجمه کن.\n"
     "فقط متن ترجمه‌شده را خروجی بده؛ توضیح نده، خلاصه نکن، به پرسش‌ها پاسخ "
     "نده و یادداشت اضافه نکن.\n"
     "ترتیب بندها، خط‌های خالی، شکست‌های خط آشکار، فهرست‌ها، شماره‌گذاری، "
     "تورفتگی، جدول‌ها و چیدمان ترمینال را حفظ کن.\n"
     "بلوک‌های کد، فرمان‌ها، نام گزینه‌ها، متغیرهای محیطی، مسیرها، URLها، "
     "جای‌نگهدارها، نام متغیرها، برچسب‌ها و ساختار Markdown را حفظ کن.\n"
     "فقط زبان طبیعی را ترجمه کن؛ کد، پیکربندی، لاگ‌ها و خروجی فرمان‌ها را "
     "وقتی ترجمه به معنا آسیب می‌زند بدون تغییر نگه دار.\n"
     "نثر معمولی را طبیعی ترجمه کن، اما بندها را ادغام نکن، محتوا را جابه‌جا "
     "نکن و خروجی را به قالبی تازه تغییر نده."},
    {"fr",
     "Français",
     "Traduisez le texte d’entrée en français.\n"
     "Ne produisez que le texte traduit. N’expliquez pas, ne résumez pas, ne "
     "répondez pas aux questions et n’ajoutez pas de notes.\n"
     "Conservez l’ordre des paragraphes, les lignes vides, les retours à la "
     "ligne explicites, les listes, la numérotation, les indentations, les "
     "tableaux et la disposition du terminal.\n"
     "Conservez les blocs de code, les commandes, les noms d’options, les "
     "variables d’environnement, les chemins, les URL, les espaces réservés, "
     "les noms de variables, les balises et la structure Markdown.\n"
     "Traduisez seulement la langue naturelle ; gardez le code, la "
     "configuration, les journaux et les sorties de commande inchangés lorsque "
     "les traduire casserait le sens.\n"
     "Rendez la prose ordinaire naturelle, mais ne fusionnez pas les "
     "paragraphes, ne réordonnez pas le contenu et ne transformez pas la sortie "
     "en un nouveau format."},
    {"he",
     "עברית",
     "תרגם את טקסט הקלט לעברית.\n"
     "הוצא רק את הטקסט המתורגם; אל תסביר, אל תסכם, אל תענה על שאלות ואל תוסיף "
     "הערות.\n"
     "שמור על סדר הפסקאות, שורות ריקות, מעברי שורה ברורים, רשימות, מספור, "
     "הזחות, טבלאות ופריסת מסוף.\n"
     "שמור על בלוקי קוד, פקודות, שמות אפשרויות, משתני סביבה, נתיבים, כתובות "
     "URL, מצייני מקום, שמות משתנים, תגיות ומבנה Markdown.\n"
     "תרגם רק שפה טבעית; השאר קוד, תצורה, לוגים ופלט פקודות ללא שינוי כאשר "
     "תרגום יפגע במשמעות.\n"
     "נסח פרוזה רגילה באופן טבעי, אך אל תמזג פסקאות, אל תסדר מחדש תוכן ואל "
     "תהפוך את הפלט לפורמט חדש."},
    {"hi",
     "हिन्दी",
     "इनपुट पाठ का हिन्दी में अनुवाद करें।\n"
     "केवल अनूदित पाठ आउटपुट करें; व्याख्या, सारांश, प्रश्नों के उत्तर या "
     "अतिरिक्त टिप्पणियाँ न जोड़ें।\n"
     "मूल अनुच्छेद क्रम, खाली पंक्तियाँ, स्पष्ट लाइन ब्रेक, सूचियाँ, क्रमांकन, "
     "इंडेंटेशन, तालिकाएँ और टर्मिनल लेआउट सुरक्षित रखें।\n"
     "कोड ब्लॉक, कमांड, विकल्प नाम, environment variables, पाथ, URL, "
     "प्लेसहोल्डर, वेरिएबल नाम, टैग और Markdown संरचना सुरक्षित रखें।\n"
     "केवल प्राकृतिक भाषा का अनुवाद करें; कोड, कॉन्फ़िग, लॉग और कमांड आउटपुट "
     "को तब अपरिवर्तित रखें जब उनका अनुवाद अर्थ बिगाड़ दे।\n"
     "सामान्य गद्य को स्वाभाविक बनाएं, लेकिन अनुच्छेद न मिलाएं, सामग्री का "
     "क्रम न बदलें और आउटपुट को नए फ़ॉर्मैट में न बदलें।"},
    {"id",
     "Bahasa Indonesia",
     "Terjemahkan teks masukan ke dalam Bahasa Indonesia.\n"
     "Keluarkan hanya teks terjemahan. Jangan menjelaskan, meringkas, menjawab "
     "pertanyaan, atau menambahkan catatan.\n"
     "Pertahankan urutan paragraf, baris kosong, line break yang jelas, daftar, "
     "penomoran, indentasi, tabel, dan tata letak terminal.\n"
     "Pertahankan blok kode, perintah, nama opsi, variabel lingkungan, path, "
     "URL, placeholder, nama variabel, tag, dan struktur Markdown.\n"
     "Terjemahkan hanya bahasa alami; biarkan kode, konfigurasi, log, dan "
     "output perintah tidak berubah bila terjemahan akan merusak makna.\n"
     "Buat prosa biasa terasa alami, tetapi jangan menggabungkan paragraf, "
     "mengurutkan ulang konten, atau mengubah output menjadi format baru."},
    {"it",
     "Italiano",
     "Traduci il testo di input in italiano.\n"
     "Produci solo il testo tradotto. Non spiegare, non riassumere, non "
     "rispondere a domande e non aggiungere note.\n"
     "Preserva l’ordine dei paragrafi, le righe vuote, gli a capo espliciti, "
     "gli elenchi, la numerazione, i rientri, le tabelle e il layout del "
     "terminale.\n"
     "Preserva blocchi di codice, comandi, nomi delle opzioni, variabili "
     "d’ambiente, percorsi, URL, segnaposto, nomi di variabili, tag e struttura "
     "Markdown.\n"
     "Traduci solo il linguaggio naturale; lascia invariati codice, "
     "configurazioni, log e output dei comandi quando tradurli ne romperebbe "
     "il significato.\n"
     "Rendi naturale la prosa ordinaria, ma non unire paragrafi, non riordinare "
     "contenuti e non trasformare l’output in un nuovo formato."},
    {"ja",
     "日本語",
     "入力テキストを日本語に翻訳してください。\n"
     "翻訳文だけを出力してください。説明、要約、質問への回答、追加の注記は"
     "しないでください。\n"
     "元の段落順、空行、明示的な改行、箇条書き、番号、インデント、表、"
     "端末レイアウトを保持してください。\n"
     "コードブロック、コマンド、オプション名、環境変数、パス、URL、"
     "プレースホルダー、変数名、タグ、Markdown 構造を保持してください。\n"
     "自然言語だけを翻訳してください。コード、設定、ログ、コマンド出力は、"
     "翻訳すると意味が壊れる場合はそのまま残してください。\n"
     "通常の文章は自然に訳してかまいませんが、段落を結合したり、内容を並べ"
     "替えたり、出力を新しい形式に変えたりしないでください。"},
    {"ko",
     "한국어",
     "입력 텍스트를 한국어로 번역하세요.\n"
     "번역문만 출력하세요. 설명, 요약, 질문에 대한 답변, 추가 메모를 넣지 "
     "마세요.\n"
     "원문의 문단 순서, 빈 줄, 명확한 줄바꿈, 목록, 번호, 들여쓰기, 표, "
     "터미널 레이아웃을 유지하세요.\n"
     "코드 블록, 명령, 옵션 이름, 환경 변수, 경로, URL, 자리표시자, 변수명, "
     "태그, Markdown 구조를 유지하세요.\n"
     "자연어만 번역하세요. 코드, 설정, 로그, 명령 출력은 번역하면 의미가 "
     "깨질 때 그대로 두세요.\n"
     "일반 문장은 자연스럽게 번역하되 문단을 합치거나 내용을 재정렬하거나 "
     "출력을 새 형식으로 바꾸지 마세요."},
    {"ms",
     "Bahasa Melayu",
     "Terjemahkan teks input ke dalam Bahasa Melayu.\n"
     "Keluarkan hanya teks terjemahan. Jangan jelaskan, ringkaskan, jawab "
     "soalan atau tambah nota.\n"
     "Kekalkan susunan perenggan, baris kosong, pemisah baris yang jelas, "
     "senarai, penomboran, inden, jadual dan tata letak terminal.\n"
     "Kekalkan blok kod, arahan, nama pilihan, pemboleh ubah persekitaran, "
     "laluan, URL, pemegang tempat, nama pemboleh ubah, tag dan struktur "
     "Markdown.\n"
     "Terjemahkan bahasa semula jadi sahaja; biarkan kod, konfigurasi, log dan "
     "output arahan tidak berubah apabila terjemahan akan merosakkan makna.\n"
     "Jadikan prosa biasa semula jadi, tetapi jangan gabungkan perenggan, susun "
     "semula kandungan atau ubah output kepada format baharu."},
    {"nl",
     "Nederlands",
     "Vertaal de invoertekst naar het Nederlands.\n"
     "Geef alleen de vertaalde tekst weer. Leg niets uit, vat niet samen, "
     "beantwoord geen vragen en voeg geen opmerkingen toe.\n"
     "Behoud de oorspronkelijke volgorde van alinea's, lege regels, harde "
     "regeleinden, lijsten, nummering, inspringing, tabellen en "
     "terminallay-out.\n"
     "Behoud codeblokken, opdrachten, optienamen, omgevingsvariabelen, paden, "
     "URL's, plaatsaanduidingen, variabelenamen, tags en Markdown-structuur.\n"
     "Vertaal alleen natuurlijke taal; laat code, configuratie, logs en "
     "opdrachtuitvoer ongewijzigd wanneer vertalen de betekenis zou breken.\n"
     "Laat gewone proza natuurlijk lezen, maar voeg geen alinea's samen, "
     "herschik geen inhoud en verander de uitvoer niet naar een nieuw formaat."},
    {"pl",
     "Polski",
     "Przetłumacz tekst wejściowy na język polski.\n"
     "Zwróć wyłącznie przetłumaczony tekst. Nie wyjaśniaj, nie streszczaj, nie "
     "odpowiadaj na pytania i nie dodawaj uwag.\n"
     "Zachowaj pierwotną kolejność akapitów, puste wiersze, twarde łamania "
     "wierszy, listy, numerację, wcięcia, tabele i układ terminala.\n"
     "Zachowaj bloki kodu, polecenia, nazwy opcji, zmienne środowiskowe, "
     "ścieżki, adresy URL, symbole zastępcze, nazwy zmiennych, tagi i strukturę "
     "Markdown.\n"
     "Tłumacz tylko język naturalny; zostaw kod, konfigurację, logi i wyjście "
     "poleceń bez zmian, gdy tłumaczenie zepsułoby znaczenie.\n"
     "Zwykłą prozę tłumacz naturalnie, ale nie łącz akapitów, nie zmieniaj "
     "kolejności treści i nie przekształcaj wyjścia w nowy format."},
    {"pt",
     "Português",
     "Traduza o texto de entrada para português.\n"
     "Produza apenas o texto traduzido. Não explique, não resuma, não responda "
     "a perguntas nem adicione notas.\n"
     "Preserve a ordem original dos parágrafos, linhas em branco, quebras de "
     "linha explícitas, listas, numeração, indentação, tabelas e layout do "
     "terminal.\n"
     "Preserve blocos de código, comandos, nomes de opções, variáveis de "
     "ambiente, caminhos, URLs, espaços reservados, nomes de variáveis, tags e "
     "estrutura Markdown.\n"
     "Traduza apenas linguagem natural; mantenha código, configuração, logs e "
     "saída de comandos inalterados quando traduzi-los quebraria o significado.\n"
     "Faça a prosa comum soar natural, mas não una parágrafos, não reordene o "
     "conteúdo e não transforme a saída em um novo formato."},
    {"ru",
     "Русский",
     "Переведи входной текст на русский язык.\n"
     "Выводи только переведённый текст. Не объясняй, не резюмируй, не отвечай "
     "на вопросы и не добавляй примечания.\n"
     "Сохраняй исходный порядок абзацев, пустые строки, явные переносы строк, "
     "списки, нумерацию, отступы, таблицы и терминальную раскладку.\n"
     "Сохраняй блоки кода, команды, имена параметров, переменные окружения, "
     "пути, URL, заполнители, имена переменных, теги и структуру Markdown.\n"
     "Переводи только естественный язык; оставляй код, конфигурацию, логи и "
     "вывод команд без изменений, если перевод нарушит смысл.\n"
     "Обычный текст делай естественным, но не объединяй абзацы, не меняй "
     "порядок содержимого и не превращай вывод в новый формат."},
    {"th",
     "ไทย",
     "แปลข้อความอินพุตเป็นภาษาไทย\n"
     "แสดงเฉพาะข้อความที่แปลแล้วเท่านั้น อย่าอธิบาย อย่าสรุป อย่าตอบคำถาม "
     "และอย่าเพิ่มหมายเหตุ\n"
     "รักษาลำดับย่อหน้า บรรทัดว่าง การขึ้นบรรทัดใหม่ที่ชัดเจน รายการ "
     "ลำดับเลข การเยื้อง ตาราง และเลย์เอาต์ของเทอร์มินัล\n"
     "รักษาบล็อกโค้ด คำสั่ง ชื่อตัวเลือก ตัวแปรสภาพแวดล้อม พาธ URL "
     "ตัวแทนข้อความ ชื่อตัวแปร แท็ก และโครงสร้าง Markdown\n"
     "แปลเฉพาะภาษาธรรมชาติ ปล่อยโค้ด คอนฟิก ล็อก และเอาต์พุตคำสั่งไว้เดิม "
     "เมื่อการแปลจะทำให้ความหมายเสีย\n"
     "ทำให้ร้อยแก้วทั่วไปอ่านเป็นธรรมชาติ แต่อย่ารวมย่อหน้า "
     "อย่าเรียงเนื้อหาใหม่ และอย่าเปลี่ยนเอาต์พุตเป็นรูปแบบใหม่"},
    {"tr",
     "Türkçe",
     "Girdi metnini Türkçeye çevir.\n"
     "Yalnızca çevrilmiş metni çıktı olarak ver. Açıklama, özetleme, soruları "
     "yanıtlama veya not ekleme.\n"
     "Özgün paragraf sırasını, boş satırları, açık satır sonlarını, listeleri, "
     "numaralandırmayı, girintileri, tabloları ve terminal yerleşimini koru.\n"
     "Kod bloklarını, komutları, seçenek adlarını, ortam değişkenlerini, "
     "yolları, URL'leri, yer tutucuları, değişken adlarını, etiketleri ve "
     "Markdown yapısını koru.\n"
     "Yalnızca doğal dili çevir; çevirmek anlamı bozacaksa kodu, yapılandırmayı, "
     "günlükleri ve komut çıktısını değiştirme.\n"
     "Sıradan düz yazıyı doğal çevir, ancak paragrafları birleştirme, içeriği "
     "yeniden sıralama veya çıktıyı yeni bir biçime dönüştürme."},
    {"uk",
     "Українська",
     "Переклади вхідний текст українською мовою.\n"
     "Виводь лише перекладений текст. Не пояснюй, не підсумовуй, не відповідай "
     "на запитання й не додавай приміток.\n"
     "Зберігай початковий порядок абзаців, порожні рядки, явні переноси рядків, "
     "списки, нумерацію, відступи, таблиці та термінальну розкладку.\n"
     "Зберігай блоки коду, команди, назви параметрів, змінні середовища, шляхи, "
     "URL, заповнювачі, імена змінних, теги та структуру Markdown.\n"
     "Перекладай лише природну мову; залишай код, конфігурацію, логи й вивід "
     "команд без змін, якщо переклад зламає зміст.\n"
     "Звичайну прозу перекладай природно, але не об’єднуй абзаци, не "
     "перевпорядковуй вміст і не перетворюй вивід на новий формат."},
    {"ur",
     "اردو",
     "ان پٹ متن کا اردو میں ترجمہ کریں۔\n"
     "صرف ترجمہ شدہ متن آؤٹ پٹ کریں؛ وضاحت، خلاصہ، سوالات کے جواب یا اضافی "
     "نوٹس شامل نہ کریں۔\n"
     "اصل پیراگراف کی ترتیب، خالی لائنیں، واضح لائن بریک، فہرستیں، نمبرنگ، "
     "انڈینٹیشن، جدولیں اور ٹرمینل لے آؤٹ برقرار رکھیں۔\n"
     "کوڈ بلاکس، کمانڈز، آپشن کے نام، ماحولیاتی متغیرات، راستے، URLs، پلیس "
     "ہولڈرز، متغیر نام، ٹیگز اور Markdown ساخت برقرار رکھیں۔\n"
     "صرف فطری زبان کا ترجمہ کریں؛ کوڈ، کنفگ، لاگز اور کمانڈ آؤٹ پٹ کو تب "
     "بغیر تبدیلی رکھیں جب ترجمہ معنی خراب کرے۔\n"
     "عام نثر کو فطری بنائیں، لیکن پیراگراف ضم نہ کریں، مواد کی ترتیب نہ "
     "بدلیں اور آؤٹ پٹ کو نئے فارمیٹ میں نہ بدلیں۔"},
    {"vi",
     "Tiếng Việt",
     "Dịch văn bản đầu vào sang tiếng Việt.\n"
     "Chỉ xuất văn bản đã dịch. Không giải thích, tóm tắt, trả lời câu hỏi "
     "hoặc thêm ghi chú.\n"
     "Giữ nguyên thứ tự đoạn, dòng trống, ngắt dòng rõ ràng, danh sách, đánh "
     "số, thụt lề, bảng và bố cục terminal.\n"
     "Giữ nguyên khối mã, lệnh, tên tùy chọn, biến môi trường, đường dẫn, URL, "
     "phần giữ chỗ, tên biến, thẻ và cấu trúc Markdown.\n"
     "Chỉ dịch ngôn ngữ tự nhiên; giữ nguyên mã, cấu hình, log và output lệnh "
     "khi dịch chúng sẽ làm sai nghĩa.\n"
     "Dịch văn xuôi thông thường cho tự nhiên, nhưng không gộp đoạn, không sắp "
     "xếp lại nội dung và không đổi output sang định dạng mới."},
    {"zh-CN",
     "简体中文",
     "把输入文本翻译成简体中文。\n"
     "只输出译文，不要解释、总结、回答问题或添加说明。\n"
     "保留原文的段落顺序、空行、明显换行、列表、编号、缩进、表格和终端布局。\n"
     "保留代码块、命令、参数名、环境变量、路径、URL、占位符、变量名、标签和 "
     "Markdown 结构。\n"
     "只翻译自然语言；代码、配置、日志和命令输出中会破坏语义的部分保持原样。\n"
     "普通文本可以译得自然，但不要主动合并段落、重排内容或改成新的格式。"},
    {"zh-TW",
     "繁體中文",
     "把輸入文字翻譯成繁體中文。\n"
     "只輸出譯文，不要解釋、總結、回答問題或加入說明。\n"
     "保留原文的段落順序、空白行、明顯換行、清單、編號、縮排、表格和終端機版面。\n"
     "保留程式碼區塊、命令、參數名稱、環境變數、路徑、URL、佔位符、變數名稱、標籤和 "
     "Markdown 結構。\n"
     "只翻譯自然語言；程式碼、設定、日誌和命令輸出中翻譯後會破壞語義的部分保持原樣。\n"
     "普通文字可以譯得自然，但不要主動合併段落、重排內容或改成新的格式。"},
}};

/**
 * @brief 比较两个 prompt 条目的语言 code。
 *
 * @param left 左侧条目。
 * @param right 右侧条目。
 * @return left code 排在 right code 前时返回 true。
 */
bool CompareEntryCode(const PromptEntry& left, const PromptEntry& right) {
  return left.code < right.code;
}

/**
 * @brief 查找内置 prompt 条目。
 *
 * @param language 目标语言代码。
 * @return 命中时返回内置定义，否则返回空。
 */
const BuiltinPrompt* FindBuiltinPrompt(std::string_view language) {
  for (const BuiltinPrompt& prompt : kBuiltinPrompts) {
    if (prompt.code == language) {
      return &prompt;
    }
  }

  return nullptr;
}

/**
 * @brief 查找可修改的 prompt 条目。
 *
 * @param entries 当前生效条目列表，不能为空。
 * @param language 目标语言代码。
 * @return 命中时返回条目指针，否则返回空。
 */
PromptEntry* FindEntry(std::vector<PromptEntry>* entries,
                       std::string_view language) {
  for (PromptEntry& entry : *entries) {
    if (entry.code == language) {
      return &entry;
    }
  }

  return nullptr;
}

}  // namespace

/**
 * @brief 用配置 prompt 覆盖内置表并追加自定义语言。
 *
 * AppConfig 中的 prompts 已由配置层过滤为合法 code 和非空 prompt，因此
 * 这里可以专注于合并来源和展示名称。
 *
 * @param config 已读取的配置字段。
 */
PromptRegistry::PromptRegistry(const config::AppConfig& config) {
  entries_.reserve(kBuiltinPrompts.size() + config.prompts.size());
  for (const BuiltinPrompt& prompt : kBuiltinPrompts) {
    entries_.push_back(PromptEntry{
        .code = std::string(prompt.code),
        .display_name = std::string(prompt.display_name),
        .prompt = std::string(prompt.prompt),
        .source = PromptSource::kBuiltin,
        .is_builtin = true,
    });
  }

  for (const auto& [language, configured_prompt] : config.prompts) {
    PromptEntry* entry = FindEntry(&entries_, language);
    if (entry != nullptr) {
      entry->prompt = configured_prompt;
      entry->source = PromptSource::kConfig;
      continue;
    }

    entries_.push_back(PromptEntry{
        .code = language,
        .display_name = language,
        .prompt = configured_prompt,
        .source = PromptSource::kConfig,
        .is_builtin = false,
    });
  }

  std::sort(entries_.begin(), entries_.end(), CompareEntryCode);
}

/**
 * @brief 返回当前可用目标语言列表。
 *
 * @return 按 code 排序的 prompt 条目副本。
 */
std::vector<PromptEntry> PromptRegistry::ListLanguages() const {
  return entries_;
}

/**
 * @brief 解析目标语言当前生效 prompt。
 *
 * @param language 目标语言代码或自定义语言标识。
 * @return 语言存在时返回条目副本，否则返回空。
 */
std::optional<PromptEntry> PromptRegistry::Resolve(
    std::string_view language) const {
  for (const PromptEntry& entry : entries_) {
    if (entry.code == language) {
      return entry;
    }
  }

  return std::nullopt;
}

/**
 * @brief 判断语言 code 是否是内置语言。
 *
 * @param language 目标语言代码。
 * @return 内置表包含该 code 时返回 true。
 */
bool PromptRegistry::HasBuiltinLanguage(std::string_view language) const {
  return FindBuiltinPrompt(language) != nullptr;
}

/**
 * @brief 将 prompt 来源转换为稳定展示标识。
 *
 * @param source prompt 来源。
 * @return `builtin` 或 `config`。
 */
std::string_view PromptSourceName(PromptSource source) {
  switch (source) {
    case PromptSource::kBuiltin:
      return "builtin";
    case PromptSource::kConfig:
      return "config";
  }

  return "builtin";
}

}  // namespace termtrans::prompt
