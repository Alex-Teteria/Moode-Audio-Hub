// ==========================================
// ATtiny13:
// PB0 (Ніжка 5) -> Вихід: Керування MOSFET (Active "0")
// PB1 (Ніжка 6) -> Вхід 1: Download complete (Active "1")
// PB2 (Ніжка 7) -> Вхід 2: Shutdown complete (Active "0")
// PB3 (Ніжка 2) -> Вхід 3: Switch on (Active "0")
// PB4 (Ніжка 3) -> Вихід: Команда Shutdown для Pi 4
// PB5 (Ніжка 1) -> Вихід: LED зелений (через резистор 300 Ом на GND)
// ==========================================

#define F_CPU 1200000UL
#include <avr/io.h>
#include <util/delay.h>

// Визначення станів
typedef enum {
    STATE_OFF,
    STATE_DOWNLOADING,
    STATE_ON,
    STATE_SHUTDOWN,
    STATE_DELAY_OFF,         // Пауза 6 сек (живлення подано)
    STATE_POWER_OFF_DELAY    // Пауза 2 сек (живлення ВИМКНЕНО)
} State_t;

int main(void) {
    // 1. БЕЗПЕЧНА ІНІЦІАЛІЗАЦІЯ ДЛЯ P-MOSFET
    // Спочатку записуємо "1" на PB0, щоб MOSFET був закритий
    PORTB |= (1 << PB0);

    // PB0, PB4 та PB5 (LED) - виходи
    DDRB |= (1 << PB0) | (1 << PB4) | (1 << PB5);

    // Початковий стан PB4 = 0 (команда Shutdown неактивна)
    PORTB &= ~(1 << PB4);

    // Початковий стан LED = 0 (вимкнено)
    PORTB &= ~(1 << PB5);

    // Налаштування вхідних пінів
    DDRB &= ~((1 << PB1) | (1 << PB2) | (1 << PB3));

    // Внутрішня підтяжка для активних "0" (PB2, PB3)
    PORTB |= (1 << PB2) | (1 << PB3);

    State_t current_state = STATE_OFF;
    uint16_t delay_counter = 0; // Спільний лічильник для таймерів автомата
    uint8_t led_counter = 0;    // Лічильник для програмного таймера блимання LED

    while (1) {
        // Зчитування та вирівнювання логіки входів
        uint8_t download_complete = (PINB & (1 << PB1)) ? 1 : 0;     // Active "1"
        uint8_t shutdown_complete  = (!(PINB & (1 << PB2))) ? 1 : 0; // Active "0"
        uint8_t switch_on          = (!(PINB & (1 << PB3))) ? 1 : 0; // Active "0"

        // 2. ОБРОБКА ПЕРЕХОДІВ ЦИФРОВОГО АВТОМАТА
        switch (current_state) {
            case STATE_OFF:
                if (switch_on && shutdown_complete) {
                    current_state = STATE_DOWNLOADING;
                }
                break;

            case STATE_DOWNLOADING:
                if (download_complete) {
                    current_state = STATE_ON;
                }
                break;

            case STATE_ON:
                if (!switch_on) {
                    current_state = STATE_SHUTDOWN;
                }
                break;

            case STATE_SHUTDOWN:
                if (shutdown_complete) {
                    delay_counter = 0;               // Скидаємо лічильник для першого таймера
                    current_state = STATE_DELAY_OFF; // Переходимо в паузу 6 секунд
                }
                break;

            case STATE_DELAY_OFF:
                if (delay_counter >= 600) {          // 600 тактів * 10 мс = 6000 мс (6 секунд)
                    delay_counter = 0;               // Скидаємо лічильник для НАСТУПНОГО таймера
                    current_state = STATE_POWER_OFF_DELAY; // Переходимо в стан знеструмлення
                } else {
                    delay_counter++;
                }
                break;

            case STATE_POWER_OFF_DELAY:
                if (delay_counter >= 200) {          // 200 тактів * 10 мс = 2000 мс (2 секунди)
                    current_state = STATE_OFF;       // Тільки тепер повертаємось в OFF
                } else {
                    delay_counter++;                 // Рахуємо 2 секунди без живлення
                }
                break;
        }

        // 3. КЕРУВАННЯ СИЛОВИМ P-MOSFET
        if (current_state == STATE_DOWNLOADING || current_state == STATE_ON || 
            current_state == STATE_SHUTDOWN    || current_state == STATE_DELAY_OFF) {
            PORTB &= ~(1 << PB0); // MOSFET ВІДКРИТИЙ (Pi працює)
        } else {
            PORTB |= (1 << PB0);  // MOSFET ЗАКРИТИЙ (Pi ЗНЕСТРУМЛЕНА)
        }

        // 4. КОМАНДА SHUTDOWN ДЛЯ PI 4
        if (current_state == STATE_SHUTDOWN) {
            PORTB |= (1 << PB4);
        } else {
            PORTB &= ~(1 << PB4);
        }

        // 5. КЕРУВАННЯ СВІТЛОДІОДОМ (LED) НА PB5 (ВИВІД 1)
        led_counter++; // Інкремент кожні 10 мс

        switch (current_state) {
            case STATE_OFF:
            case STATE_POWER_OFF_DELAY:
                PORTB &= ~(1 << PB5); // Вимкнено постійно
                led_counter = 0;
                break;

            case STATE_ON:
                PORTB |= (1 << PB5);  // Світиться постійно
                led_counter = 0;
                break;

            case STATE_DOWNLOADING:
                // Повільне блимання: кожні 500 мс (50 тактів по 10 мс) змінюємо стан
                if (led_counter >= 50) {
                    PORTB ^= (1 << PB5); // Інвертуємо стан піна (XOR)
                    led_counter = 0;     // Скидаємо лічильник
                }
                break;

            case STATE_SHUTDOWN:
            case STATE_DELAY_OFF:
                // Швидке блимання: кожні 100 мс (10 тактів по 10 мс) змінюємо стан
                if (led_counter >= 10) {
                    PORTB ^= (1 << PB5); // Інвертуємо стан піна (XOR)
                    led_counter = 0;     // Скидаємо лічильник
                }
                break;
        }

        // Базовий такт автомата
        _delay_ms(10);
    }
}
