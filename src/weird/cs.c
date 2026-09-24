#define CS_STD_IMPLEMENTATION
#include "cs.h"

int main(void) {
    // 1. Ініціалізуємо основання стеку для GC
    GC.Init();

    Console.WriteLine("=== Тест C# Style Library для C ===");
    Console.WriteLine("Поточний час: %lld ms", (long long)Time.Now());

    // 2. Ввід з консолі
    Console.Write("\nВведіть ваше ім'я: ");
    char* name = Console.ReadLine();
    Console.WriteLine("Привіт, %s!", name);

    // 3. Запис та читання файлу
    const char* filename = "demo_test.txt";
    Console.WriteLine("\nЗаписуємо у файл '%s'...", filename);
    File.Write(filename, "Hello from C# style C library!\nSecond line of content.");

    if (File.Exists(filename)) {
        Console.WriteLine("Файл знайдено. Читаємо вміст:");
        char* content = File.Read(filename);
        Console.WriteLine("--------------------------------");
        Console.WriteLine("%s", content);
        Console.WriteLine("--------------------------------");
    }

    // 4. Демонстрація роботи GC
    Console.WriteLine("\nСтворюємо 1000 тимчасових рядків у циклі...");
    for (int i = 0; i < 1000; i++) {
        // Кожен ReadLine або Alloc виділяє пам'ять
        char* temp = (char*)GC.Alloc(512);
        snprintf(temp, 512, "Тимчасовий рядок №%d", i);
    }

    Console.WriteLine("Запускаємо примусовий GC.Collect()...");
    GC.Collect();
    Console.WriteLine("Пам'ять успішно очищено без витоків!");

    return 0;
}