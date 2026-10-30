# Kernel Data Structures — IV-2-4

| | |
|---|---|
| **Хичээл** | F.CSM302 Үйлдлийн систем — Бие даалт 1 (БД1) |
| **Хуваарилалтын код** | **IV-2-4** |
| **Төсөл** | Kernel Data Structures (OS Concepts 10e, Chapter 3, Programming Project 4) |
| **Үндсэн UI дүрслэл** | Kernel structure graph (элемент нэмэх / устгах / хайх үйлдлийн дараах бүтцийг графаар харуулна) |
| **Гишүүд** | B200900028 [нэр] · B221910021 [нэр] · B231910034 [нэр] · B232270805 [нэр] · B241940102 [нэр] · B242270075 [нэр] |
| **GitHub** | https://github.com/Ulmj7/Operating_System_BD1 |

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

> **Анхаарах:** энэ компьютерт kernel header байхгүй тул жинхэнэ `.ko` файлыг **бүтээж, `insmod`
> хийж туршаагүй**. Module-ийн эх кодыг user-space дахь өөрсдийн `linux/*.h` shim (`src/shim/`)-тэй
> **өөрчлөлтгүйгээр** хөрвүүлж, тестлэсэн. Бодит kernel дээр ажиллуулах заавар `docs/IV-2-4_Manual.pdf`-д.

## Quick Start

Шаардлага: Linux, `gcc`, `make`, `python3` (зөвхөн стандарт сан), орчин үеийн browser. Туршсан хувилбар: Arch Linux, gcc 16.2.1, GNU Make 4.4.1, Python 3.14.8, Brave 156.
Гаднаас сан суулгах, root эрх, интернет **шаардлагагүй**.

```sh
git clone https://github.com/Ulmj7/Operating_System_BD1.git kernel-data-structures
cd kernel-data-structures
make            # build/kds_engine болон build/libkds.a-г бүтээнэ
make test       # бүх тест (C unit, engine cases, HTTP server; ASan+UBSan) — алдаатай бол 0-ээс өөр кодтой гарна
make run        # UI-г http://127.0.0.1:8080 дээр асаана (зөвхөн локал)
```

Browser-ээр `http://127.0.0.1:8080` нээгээд **Color / Collatz / Stack / Queue** табуудаас үйлдэл
хийнэ. Дэлгэрэнгүй нь `docs/IV-2-4_Manual.pdf`. Жишээ нь URL-ээр автоматаар ажиллуулж болно:

```
http://127.0.0.1:8080/?run=color%20example;color%20delete%20index=1
```

Жинхэнэ Linux kernel дээр (kernel header-тэй машинд): `make module` (`src/kernel/Makefile`, Kbuild).

## Репозиторийн бүтэц

```
README.md
Makefile                 make / make test / make run / make module / make clean
src/shim/                user-space "linux/*.h" shim: list.h (өөрийн хэрэгжилт), kmalloc/kfree, printk, module_param
src/kernel/              БОДИТ kernel module-ийн эх код: color_list.c, collatz.c, Kbuild Makefile
src/engine/              kds_core.{h,c}, main.c -> build/kds_engine (stdin команд -> нэг мөр JSON)
src/ui/server.py         python3 stdlib HTTP сервер (127.0.0.1), нэг тогтвортой engine процесс
src/ui/static/           index.html, app.js, style.css — SVG граф, офлайн, light/dark
tests/                   test_*.c, cases/*.in + expected, run_cases.py, test_server.py, README.md
docs/                    IV-2-4_BD1_Report.pdf, IV-2-4_Manual.pdf, IV-2-4_Teams_Post.md, img/ (бодит screenshot), src/ (PDF-ийн HTML эх)
```

## Тест

`make test` нь дараахыг ажиллуулна (үр дүн, хүлээгдсэн утга бүрийн гарал үүсэл — `tests/README.md`):

* `tests/test_list.c` — өөрийн `list.h`-ийн unit тест + seed-тэй санамсаргүй differential тест
* `tests/test_color_module.c`, `tests/test_collatz_module.c` — **жинхэнэ** module `.c`-г shim-тэй хөрвүүлж,
  `printk` гаралт, `kmalloc == kfree`, `kmalloc` бүрийн байрлалд ENOMEM тарьж leak шалгана
* `tests/test_core.c` — engine-ийн бүх команд, зориудаар эвдсэн жагсаалтыг `kds_verify` барьж буй эсэх
* `tests/run_cases.py` — `tests/cases/*.in` + хүлээгдсэн JSON (ном дахь жишээ, boundary/error кейсүүд)
* `tests/test_server.py` — HTTP сервер (400 кейс, path traversal, restart, loopback-only bind)
* `tests/ui/browser_check.py`, `tests/ui/interactions.py` — **заавал биш** Brave/Chromium (CDP) UI шалгалт; `make test`-д ороогүй (`make run`-ийн дараа гараар ажиллуулна, `tests/README.md`)

## AI болон бэлэн кодын ашиглалт (тэмдэглэл)

* Энэ төслийн эх код, тест, баримт бичгийн ноорог нь **AI агентуудын тусламжтай** бичигдсэн
  (Anthropic Claude болон OpenAI Codex загварууд дээр суурилсан олон агентын ажлын урсгал:
  tech lead, developer, qa-reviewer). Код нь автомат тестээр баталгаажсан; багийн гишүүд өөрсдөө
  уншиж, `make test`-ийг дахин ажиллуулж хянах ёстой. Дэлгэрэнгүйг тайлангийн 5.8 хэсгээс үз.
* Linux-ийн `<linux/list.h>`-ийг **хуулаагүй**; `src/shim/linux/list.h` нь ижил нэр бүхий макроны
  өөрсдийн хэрэгжилт юм. Гуравдагч талын сан ашиглаагүй (зөвхөн C стандарт сан, POSIX, python3 stdlib).
* Сурах бичгийн snippet-д violet өнгийг `blue=43, green=226` гэж бичсэн нь struct-ийн
  талбарын дараалал (red, blue, green)-тай RGB (138, 43, 226)-г андуурсан байна. Бид зөв утгыг
  (red=138, green=43, blue=226) ашигласан.
* Нууц түлхүүр, нууц үг, хувийн өгөгдөл репозиторид **байхгүй**.

## Лиценз

Сургалтын даалгаврын код. GPL-тэй нийцүүлэхийн тулд kernel module-ууд `MODULE_LICENSE("GPL")`.
