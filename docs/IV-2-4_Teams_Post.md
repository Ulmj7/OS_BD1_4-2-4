# БД1 пост — Teams «БД1 - Submissions and Reviews» сувагт (хуулж тавих текст)

> Баг бүр зөвхөн **нэг** пост хэрэглэнэ (Guide §2).
> Хавсаргах файлууд: `docs/IV-2-4_BD1_Report_final.pdf`, `docs/IV-2-4_Manual_final.pdf`.

**Гарчиг**

```
IV-2-4 – Kernel Data Structures
```

**Тайлбар**

```
Код: IV-2-4
Гишүүд: Б. Лхамрагчаа (B241940102), Г. Үлэмж (B232270805), Т. Буянтөгс (B231910034), Э. Төгсбаяр (221910021), Ө. Чинзориг (200900028), Б. Идэрбат (B242270075)
Төсөл: Kernel Data Structures (OS Concepts 10e, 3-р бүлэг, Programming Project 4) — UI: Kernel structure graph
Товч үр дүн: Номын Part I (4 өнгөтэй list_head жагсаалт) болон Part II (collatz module, start параметр)-ийн жинхэнэ module эх кодыг user-space shim дээр өөрчлөлтгүй ажиллуулж, make test-ээр (5,000 санамсаргүй үйлдэл, 1,254 ENOMEM байрлал, Collatz 27 → 112 ба 837799 → 525 зангилаа) leak-гүй, зөв болохыг шалгасан. Үйлдэл бүрийн дараах list_head бүтцийг C хөдөлгүүрийн бодит заагчаас зурдаг интерактив граф UI-тай. Модулиуд жинхэнэ кернелийн header-тэй (linux-headers 7.2.11.arch1-1) .ko болтол хөрвөж, багийн гишүүний машин дээр (Arch Linux, Linux 7.2.11-arch1-1) insmod/dmesg/rmmod-оор ачаалагдсан: өнгөний 4 мөр, Collatz start=27 үед 112 мөр (оргил 9232) shim-ийн үр дүнтэй таарсан (тайлан 8.7–8.8). Кернел дээр буруу/захын start, ENOMEM, санах ойн алдагдал (kmemleak) шалгаагүй (тайлан 9.2).
GitHub: https://github.com/Ulmj7/OS_BD1_4-2-4
Хавсралт: IV-2-4_BD1_Report_final.pdf, IV-2-4_Manual_final.pdf
Туршиж үзэх: README-ийн Quick Start хэсэг (make → make test → make run → http://127.0.0.1:8080).
```
