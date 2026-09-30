# 007 — Runtime-редагування

Setter-и приймають `&ctx`, тому що після зміни Mica повторно парсить текст і замінює контекст.

- `mica_set_string(&ctx, "config.input_path", "./src")` додає відсутнє поле.
- `mica_set_string(&ctx, "runtime.cache.path", "./cache")` створює всю відсутню гілку.
- Звичайний setter не стирає `aspect_ratio = .width / 720`; він повертає `MICA_SET_IS_EXPRESSION`.
- `mica_set_float_force` — явний дозвіл замінити такий вираз літералом.

Після редагування `mica_source(ctx)` дає оновлений текст, який застосунок може зберегти у файл.
