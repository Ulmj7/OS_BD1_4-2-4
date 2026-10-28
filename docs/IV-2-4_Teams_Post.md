# БД1 пост — Teams «БД1 - Submissions and Reviews» сувагт (хуулж тавих текст)

> Баг бүр зөвхөн **нэг** пост хэрэглэнэ (Guide §2).
> Хавсаргах файлууд: `docs/IV-2-4_BD1_Report.pdf`, `docs/IV-2-4_Manual.pdf`.

**Гарчиг**

```
IV-2-4 – Kernel Data Structures
```

**Тайлбар**

```
Код: IV-2-4
Төсөл: Kernel Data Structures (OS Concepts 10e, 3-р бүлэг, Programming Project 4) — UI: Kernel structure graph
Товч үр дүн: Номын Part I (4 өнгөтэй list_head жагсаалт) болон Part II (collatz module, start параметр)-ийн жинхэнэ module эх кодыг user-space shim дээр өөрчлөлтгүй ажиллуулж, make test-ээр (5,000 санамсаргүй үйлдэл, 1,254 ENOMEM байрлал, Collatz 27 → 112 ба 837799 → 525 зангилаа) leak-гүй, зөв болохыг шалгасан. Үйлдэл бүрийн дараах list_head бүтцийг C хөдөлгүүрийн бодит заагчаас зурдаг интерактив граф UI-тай. Анхаар: kernel header байхгүй тул жинхэнэ .ko-г бүтээж/insmod хийгээгүй (тайлан 9.2).
GitHub: https://github.com/Ulmj7/Operating_System_BD1
Хавсралт: IV-2-4_BD1_Report.pdf, IV-2-4_Manual.pdf
Туршиж үзэх: README-ийн Quick Start хэсэг (make → make test → make run → http://127.0.0.1:8080).
```
