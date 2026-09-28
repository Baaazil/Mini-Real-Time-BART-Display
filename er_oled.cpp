/***************************************************
//Web: http://www.buydisplay.com
EastRising Technology Co.,LTD
****************************************************/
#include <Arduino.h>
#include <Wire.h>
#include "er_oled.h"




void command(uint8_t cmd)
{ 
  Wire.beginTransmission(0x3c);
  Wire.write(0x00);
  Wire.write(cmd);
  Wire.endTransmission();
}

void data(uint8_t dat)
{  Wire.beginTransmission(0x3c);
  Wire.write(0x40);
  Wire.write(dat);
  Wire.endTransmission();
}


void Write_CGRAM(const uint8_t *pBmp)
{uint8_t k;
//  command(0x28);  //RE=0,IS=0
  command(0x40);
  for(k=0;k<64;k++)
  {data(pBmp[k]);
 // *pBmp++
  }
}


void Write_DDRAM(uint8_t a)
{uint8_t j;
er_oled_set_cursor(0,0,0);
 command(0x02);
  delay(10); 
 command(0x80);
	for(j=0;j<40;j++)
	{data(a);
	}
 command(0xc0);
	for(j=0;j<40;j++)
	{data(a);
	}
}

 void OLED_Display_Turn(uint8_t dat)
{
	command(0x2A);   //RE=1
	if(dat==0)
	{	
	command(0x06);   // SEG99->SEG0  	 COM0->COM31
	}
	else
	{
	command(0x05);   // SEG0->SEG99    COM31->COM0
	}
	command(0x28);
}




void er_oled_begin()
{

    pinMode(OLED_RST, OUTPUT);

    
    digitalWrite(OLED_RST, HIGH);
    delay(100);
    digitalWrite(OLED_RST, LOW);
    delay(100);
    digitalWrite(OLED_RST, HIGH);
     delay(100);
 
   command  (0x2a);  //RE=1
    command  (0x71);  //Function Selection A
    data(0x00);	//Disable internal VDD
    command  (0x28);  //RE=0,IS=0
    command  (0x08);  //display OFF

    command  (0x2a);  //RE=1
    command  (0x79);  //SD=1  OLED command set is enabled
    command  (0xD5);  //Set Display Clock Divide Ratio/ Oscillator Frequency 
    command  (0x70);  
    command  (0x78);  //SD=0   OLED command set is disabled
    command  (0x09);  //5-dot font width, black/white inverting of cursor disable, 1-line or 2-line display mode
    command  (0x06);  //COM0 -> COM31  SEG99 -> SEG0,
    command  (0x72);  //Function Selection B. Select the character no. of character generator    Select character ROM
    data(0x01);  //CGRAOM 248 COGRAM 8   Select  ROM A

    command  (0x2a);  //RE=1
    command  (0x79);  //SD=1  OLED command set is enabled
    command  (0xDA);  //Set SEG Pins Hardware Configuration  
    command  (0x10);  //Alternative (odd/even) SEG pin configuration, Disable SEG Left/Right remap
    command  (0xDC);  //Function Selection C  Set VSL & GPIO   
    command  (0x83);  //Internal VSL  represents GPIO pin HiZ, input disabled (always read as low)
    command  (0x81);  //Set Contrast Control   
    command  (0x8F);  
    command  (0xD9);  //Set Phase Length   
    command  (0x73);  
    command  (0xDB);  //Set VCOMH Deselect Level (  
    command  (0x30);  //0.83 x VCC

    command  (0x78);  //SD=0   OLED command set is disabled
    command  (0x28);  //RE=0,IS=0

    er_oled_clear();  //Clear Display
    command  (0x80);  //Set DDRAM Address

    command  (0x0C);
    command  (0x02); 
    
       
    Write_CGRAM(cgram);
}

void er_oled_set_cursor(uint8_t x,uint8_t y,uint8_t Display_direction)
{      uint8_t addr=0;
 OLED_Display_Turn(Display_direction);
 
	addr+=x+y*0x20;
	command(0x28);
	command(0x80|addr); 
}

void er_oled_clear(void)
{ //  command(0x28);  //RE=0,IS=0
    command(0x01); 
     delay(10);
}

void er_oled_string(uint8_t x,uint8_t y, const char * pString,uint8_t Display_direction)
{ uint8_t character; 
  er_oled_set_cursor( x,y,Display_direction);
 while (*pString != '\0')
  {character=(*pString);
    data(character);
   *pString++;
  }
}

void er_oled_char(uint8_t x,uint8_t y, uint8_t character,uint8_t Display_direction)
{ 
  er_oled_set_cursor( x,y,Display_direction);
  data(character);
}

