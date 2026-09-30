# Mica how-to

Приклади розташовані від найпростішого до складнішого. У кожній папці є `config.mica`, `main.c` та власний `README.md`.

1. `001_example_read_literals` — числа, bool і рядки.
2. `002_example_nested_objects` — ієрархічні шляхи.
3. `003_example_expressions` — залежності, умови й функції.
4. `004_example_strings` — інтерполяція та raw-рядки.
5. `005_example_object_merging` — успадкування об'єктів через `+`.
6. `006_example_fallbacks` — fallback, помилки й цикли.
7. `007_example_runtime_edits` — безпечна зміна й створення значень із C.

З кореня проєкту будь-який приклад компілюється так:

```sh
cc -std=c99 -Wall -Wextra -pedantic mica.c how_to/001_example_read_literals/main.c -o tutorial
```

`main.c` містить конфіг як C-рядок, щоб приклад був самодостатнім для компіляції. Файл `config.mica` — той самий текст у природному форматі, який зазвичай завантажує ваш застосунок.
