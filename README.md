# Kernel Data Structures — IV-2-4

| | |
|---|---|
| **Хичээл** | F.CSM302 Үйлдлийн систем — Бие даалт 1 (БД1) |
| **Хуваарилалтын код** | **IV-2-4** |
| **Төсөл** | Kernel Data Structures (OS Concepts 10e, Chapter 3, Programming Project 4) |
| **Үндсэн UI дүрслэл** | Kernel structure graph (элемент нэмэх / устгах / хайх үйлдлийн дараах бүтцийг графаар харуулна) |
| **Гишүүд** | Б. Лхамрагчаа (B241940102) · Г. Үлэмж (B232270805) · Т. Буянтөгс (B231910034) · Э. Төгсбаяр (221910021) · Ө. Чинзориг (200900028) · Б. Идэрбат (B242270075) |
| **GitHub** | https://github.com/Ulmj7/OS_BD1_4-2-4 |

## Зорилго

Linux kernel-ийн дугуй (circular) хоёр чиглэлт холбоост жагсаалт `struct list_head`
(`<linux/list.h>`)-ийг сурах бичгийн даалгаврын дагуу хэрэгжүүлж, түүний үйлдлүүдийг
(`list_add_tail`, `list_for_each_entry`, `list_del`, `list_for_each_entry_safe`, `kmalloc`/`kfree`)
хөрвүүлэгдэх бодит кодоор турших, мөн **бодит ажиллаж буй төлөвөөс** зурагддаг интерактив граф
харуулах.

Сурах бичгийн шаардлага:

* **Part I** — `struct color {red, blue, green, list}` бүхий 4 элементтэй жагсаалт үүсгэж, dmesg-д
  хэвлэх; module-ээс гарахад `list_del` + `kfree`-ээр бүгдийг чөлөөлөх (`src/kernel/color_list.c`).
* **Part II** — `collatz` module: `start` параметр (default 25) авч, Collatz дарааллыг kernel
  жагсаалтад хадгалж, хэвлэж, гарахад чөлөөлөх (`src/kernel/collatz.c`).

> **Бодит кернел дээр:** `color_list.ko` ба `collatz.ko`-г `make module KDIR=/usr/lib/modules/$(uname -r)/build`-ээр
> бүтээж (linux-headers 7.2.11.arch1-1), багийн гишүүн өөрийн машин дээр (Arch Linux, Linux 7.2.11-arch1-1)
> 2026-10-30-нд `sudo insmod` → `dmesg` → `sudo rmmod`-оор ачаалж туршсан: өнгөний 4 мөр, `collatz start=27`
> үед 112 мөр (оргил 9232) гарч, shim дээрх үр дүнтэй таарсан. Харин кернел дээр
> буруу/захын `start`, ENOMEM, санах ойн алдагдал (kmemleak) шалгаагүй. Module-ийн эх кодыг user-space дахь
> өөрсдийн `linux/*.h` shim (`src/shim/`)-тэй **өөрчлөлтгүйгээр** хөрвүүлж тестлэдэг (`make test` кернелд ачаалдаггүй).
> Дэлгэрэнгүй: тайлан 8.7–8.8, `docs/IV-2-4_Manual_final.pdf` 8.2.7.

## Quick Start

Шаардлага: Linux, `gcc`, `make`, `python3` (зөвхөн стандарт сан), орчин үеийн browser. Туршсан хувилбар: Arch Linux, gcc 16.2.1, GNU Make 4.4.1, Python 3.14.8, Brave 156.
Гаднаас сан суулгах, root эрх, интернет **шаардлагагүй**.

```sh
git clone https://github.com/Ulmj7/OS_BD1_4-2-4.git kernel-data-structures
cd kernel-data-structures
make            # build/kds_engine болон build/libkds.a-г бүтээнэ
make test       # бүх тест (C unit, engine cases, HTTP server; ASan+UBSan) — алдаатай бол 0-ээс өөр кодтой гарна
make run        # UI-г http://127.0.0.1:8080 дээр асаана (зөвхөн локал)
```

Browser-ээр `http://127.0.0.1:8080` нээгээд **Color / Collatz / Stack / Queue** табуудаас үйлдэл
хийнэ. Дэлгэрэнгүй нь `docs/IV-2-4_Manual_final.pdf`. Жишээ нь URL-ээр автоматаар ажиллуулж болно:

```
http://127.0.0.1:8080/?run=color%20example;color%20delete%20index=1
```

Жинхэнэ Linux кернелд (кернелийн header-тэй машинд; дэлгэрэнгүй `docs/IV-2-4_Manual_final.pdf` 8.2.7):

```sh
make module KDIR=/usr/lib/modules/$(uname -r)/build     # src/kernel/color_list.ko, collatz.ko (src/kernel/Makefile, Kbuild)
sudo insmod src/kernel/collatz.ko start=27 ; sudo dmesg | tail -130 ; sudo rmmod collatz
```

Анхаар: кернел модуль ажиллаж буй кернелд ачаалагдана; ажлаа хадгалж, боломжтой бол виртуал машин ашиглана.

## Репозиторийн бүтэц

```
README.md
Makefile                 make / make test / make run / make module / make clean
src/shim/                user-space "linux/*.h" shim: list.h (өөрийн хэрэгжилт), kmalloc/kfree, printk, module_param
src/kernel/              БОДИТ кернел модулийн эх код: color_list.c, collatz.c, Kbuild Makefile
src/engine/              kds_core.{h,c}, main.c -> build/kds_engine (stdin команд -> нэг мөр JSON)
src/ui/server.py         python3 stdlib HTTP сервер (127.0.0.1), нэг тогтвортой engine процесс
src/ui/static/           index.html, app.js, style.css — SVG граф, офлайн, light/dark
tests/                   test_*.c, cases/*.in + expected, run_cases.py, test_server.py, README.md
video/                   IV-2-4_BD1_Demo.mp4 — танилцуулах видео
docs/                    IV-2-4_BD1_Report_final.pdf, IV-2-4_Manual_final.pdf, IV-2-4_Teams_Post.md, img/ (бодит screenshot), src/ (PDF-ийн HTML эх)
```

## Тест

`make test` нь дараахыг ажиллуулна (үр дүн, хүлээгдсэн утга бүрийн гарал үүсэл — `tests/README.md`):

* `tests/test_list.c` — өөрийн `list.h`-ийн unit тест + seed-тэй санамсаргүй differential тест
* `tests/test_color_module.c`, `tests/test_collatz_module.c` — **жинхэнэ** module `.c`-г shim-тэй хөрвүүлж,
  `printk` гаралт, `kmalloc == kfree`, `kmalloc` бүрийн байрлалд ENOMEM тарьж leak шалгана
* `tests/test_core.c` — engine-ийн бүх команд, зориудаар эвдсэн жагсаалтыг `kds_verify` барьж буй эсэх
* `tests/run_cases.py` — `tests/cases/*.in` + хүлээгдсэн JSON (ном дахь жишээ, boundary/error кейсүүд)
* `tests/test_server.py` — HTTP сервер, 12 тест (400 кейс, path traversal, restart, loopback-only bind, Ctrl+C-ийн цэвэр shutdown — SIGINT үл тоомсорлогдсон үед ч)
* `tests/ui/browser_check.py`, `tests/ui/interactions.py` — **заавал биш** Brave/Chromium (CDP) UI шалгалт; `make test`-д ороогүй (`make run`-ийн дараа гараар ажиллуулна, `tests/README.md`)

## Багийн гүйцэтгэл ба ашигласан хэрэгсэл

Ажлыг дээрх зургаан гишүүн гүйцэтгэсэн. Хувь хүний ажлын хуваарилалт тайлангийн Хавсралт A-д бий.
AI хэрэгслийг ноорог, код болон шалгалтын ажлын урсгалд туслах хэрэгсэл болгон ашигласан;
төслийн гүйцэтгэл, эцсийн үр дүнгийн хариуцлага багт хамаарна. Дэлгэрэнгүйг тайлангийн 5.8-аас үз.

## Бэлэн кодын ашиглалт

* Linux-ийн `<linux/list.h>`-ийг **хуулаагүй**; `src/shim/linux/list.h` нь ижил нэр бүхий макроны
  өөрсдийн хэрэгжилт юм. Гуравдагч талын сан ашиглаагүй (зөвхөн C стандарт сан, POSIX, python3 stdlib).
* Сурах бичгийн snippet-д violet өнгийг `blue=43, green=226` гэж бичсэн нь struct-ийн
  талбарын дараалал (red, blue, green)-тай RGB (138, 43, 226)-г андуурсан байна. Бид зөв утгыг
  (red=138, green=43, blue=226) ашигласан.
* Нууц түлхүүр, нууц үг репозиторид байхгүй; багийн гишүүдийн нэр, оюутны код тайланд бий.

## Лиценз

Сургалтын даалгаврын код. GPL-тэй нийцүүлэхийн тулд kernel module-ууд `MODULE_LICENSE("GPL")`.
