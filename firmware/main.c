/* Используемые библиотеки */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

/*===========================================================================*/
/* Конфигурация портов ATmega8                                               */
/*===========================================================================*/

/* Задается в 'makefile' */
//#define F_CPU 8000000L

/* Порты и пины */
#define PAD_Clock_PORT		PIND
#define PAD_Latch_PORT		PIND
#define PAD_Data_PORT		PIND

#define INPUT_RIGHT_PORT	PINC
#define INPUT_LEFT_PORT		PINC
#define INPUT_DOWN_PORT		PINC
#define INPUT_UP_PORT		PINC

#define INPUT_START_PORT	PIND
#define INPUT_SELECT_PORT	PIND
#define INPUT_B_PORT		PIND
#define INPUT_A_PORT		PINB
#define INPUT_TB_PORT		PINB
#define INPUT_TA_PORT		PINB

#define PAD_Clock_PIN		PD2
#define PAD_Latch_PIN		PD3
#define PAD_Data_PIN		PD0

#define INPUT_RIGHT_PIN		PC4
#define INPUT_LEFT_PIN		PC2
#define INPUT_DOWN_PIN		PC5
#define INPUT_UP_PIN		PC3

#define INPUT_START_PIN		PD5
#define INPUT_SELECT_PIN	PD6
#define INPUT_B_PIN		PD7
#define INPUT_A_PIN		PB0
#define INPUT_TB_PIN		PB2
#define INPUT_TA_PIN		PB1

/* Биты нажатых кнопок */
/*																			 НАЖАТА : ОТПУЩЕННА*/
#define INPUT_RIGHT		(( INPUT_RIGHT_PORT	&	_BV( INPUT_RIGHT_PIN	)) ? 0x80 : 0)
#define INPUT_LEFT		(( INPUT_LEFT_PORT	&	_BV( INPUT_LEFT_PIN		)) ? 0x40 : 0)
#define INPUT_DOWN		(( INPUT_DOWN_PORT	&	_BV( INPUT_DOWN_PIN		)) ? 0x20 : 0)
#define INPUT_UP		(( INPUT_UP_PORT	&	_BV( INPUT_UP_PIN 		)) ? 0x10 : 0)

#define INPUT_START		(( INPUT_START_PORT	&	_BV( INPUT_START_PIN	)) ? 0x08 : 0)
#define INPUT_SELECT	(( INPUT_SELECT_PORT&	_BV( INPUT_SELECT_PIN	)) ? 0x04 : 0)
#define INPUT_B			(( INPUT_B_PORT		&	_BV( INPUT_B_PIN		)) ? 0x02 : 0)
#define INPUT_A			(( INPUT_A_PORT		&	_BV( INPUT_A_PIN		)) ? 0x01 : 0)

/* Биты кнопок Турбо B A */
#define INPUT_TB		(( INPUT_TB_PORT	&	_BV( INPUT_TB_PIN		)) ? 0x02 : 0)
#define INPUT_TA		(( INPUT_TA_PORT	&	_BV( INPUT_TA_PIN		)) ? 0x01 : 0)

/* Переменные */
volatile uint8_t shift, nes_button_data = 0;

volatile uint8_t turbo_data = 0, turbo_shift = 0, turbo_pattern_shift;
volatile uint16_t turbo_pattern = 0xA995;

/* Инициализация портов и регистров */
void hwInit(void)
{
	// Инициализация портов
	PORTB	 =	( 1 << INPUT_A_PIN ) | ( 1 << INPUT_TA_PIN ) | ( 1 << INPUT_TB_PIN );
	DDRB	 =	( 0 << INPUT_A_PIN ) | ( 0 << INPUT_TA_PIN ) | ( 0 << INPUT_TB_PIN );

	PORTC	 =	( 1 << INPUT_RIGHT_PIN ) | ( 1 << INPUT_LEFT_PIN ) | ( 1 << INPUT_DOWN_PIN ) | ( 1 << INPUT_UP_PIN );
	DDRC	 =	( 0 << INPUT_RIGHT_PIN ) | ( 0 << INPUT_LEFT_PIN ) | ( 0 << INPUT_DOWN_PIN ) | ( 0 << INPUT_UP_PIN );

	PORTD	 =	( 1 << PAD_Clock_PIN ) | ( 1 << PAD_Latch_PIN ) | ( 1 << PAD_Data_PIN ) | 
				( 1 << INPUT_START_PIN ) | ( 1 << INPUT_SELECT_PIN ) | ( 1 << INPUT_B_PIN );

	DDRD	 =	( 0 << PAD_Clock_PIN	) | ( 0 << PAD_Latch_PIN ) | ( 1 << PAD_Data_PIN ) | 
				( 0 << INPUT_START_PIN	) | ( 0 << INPUT_SELECT_PIN ) | ( 0 << INPUT_B_PIN );

#if (F_CPU == 8000000L)
	//8 МГц  предделитель 1024
	TCCR2	 =	( 0 << WGM20 ) | ( 1 << WGM21) | ( 1 << CS22  ) |
			( 1 << CS21  ) | ( 1 << CS20 );

	//Регистр сравнения, время счета 3.968 мс при частоте 8 МГц
	OCR2  = 0x1F;
	TCNT2 = 0xE1;

#elif (F_CPU == 16000000L)
	// 16 МГц  предделитель 1024
	TCCR2	 =	( 0 << WGM20 ) | ( 1 << WGM21) | ( 1 << CS22  ) |
			( 1 << CS21  ) | ( 1 << CS20 );

	//Регистр сравнения, время счета 3.968 мс при частоте 16 МГц
	OCR2  = 0x3E;
	TCNT2 = 0xC2;
#endif

	//Включение прерывания при растущем Latch, Clock
	MCUCR = (1 << ISC01) | (1 << ISC00) | (1 << ISC11) | (1 << ISC10);
	GICR  = (1 << INT0 ) | (1 << INT1 );
}

/* Прерывание при растущем Clock */
ISR(INT0_vect)
{
	shift <<= 1;
	if (nes_button_data & shift)
		PORTD |= 1;
	else
		PORTD &= ~1;
}

/* Прерывание при растущем Latch */
ISR(INT1_vect)
{
	shift = 1;

	if (nes_button_data & shift)
		PORTD |= 1;
	else
		PORTD &= ~1;

	turbo_shift++;
}

/* Считать нажатые кнопки */
void buttons_read(void)
{
//Пропустить, если не прошло время 4 мс ("Защита от дребезга контактов")
if (bit_is_set(TIFR, OCF2))
	{
		// Сброс таймера отсчета
		TIFR |= _BV(OCF2);

#if (F_CPU == 8000000L)
		// 8Mhz
		OCR2  = 0x1F;
		TCNT2 = 0xE1;

#elif (F_CPU == 16000000L)
		// 16Mhz
		OCR2  = 0x3E;
		TCNT2 = 0xC2;
#endif
		/* 
			Счтитать нажатые кнопки
			Right, Left, Down, Up, Start, Select, B, A
		*/
		nes_button_data =	INPUT_UP	|	INPUT_DOWN	|	INPUT_LEFT	|	INPUT_RIGHT	|
					INPUT_B		|	INPUT_A		|
					INPUT_START	|	INPUT_SELECT;

		/* Если нажаты кнопки TB, TA */
		if (INPUT_TB != 0x02) nes_button_data = (nes_button_data & ~0x02) | (turbo_data & 0x02);
		if (INPUT_TA != 0x01) nes_button_data = (nes_button_data & ~0x01) | (turbo_data & 0x01);
	}
}

/*===========================================================================*/
/* Начало основной программы */
int main(void)
{
	hwInit();
	sei();

	while (1)
		{
			/* считать нажатые кнопки */
			buttons_read();

			/* Инверсия турбо */
			if (turbo_shift >= 1)
				{
					turbo_data = ((turbo_pattern >> turbo_pattern_shift) & 0x0001) * 3;
					// turbo_data ^= 0x03;
					turbo_shift = 0;
					turbo_pattern_shift++;
				}

			if (turbo_pattern_shift >= 0x000F)
				turbo_pattern_shift = 0;
		}
}
