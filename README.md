Ouija Board SAO designed for Hackaday SuperCon 2025 

There are two versions of the board, front lit and one backlit. 
### Front Illuminated
<img src="../SAOuija/images/frontlitFront.png" height=200> | <img src="../SAOuija/images/frontlitBack.png" height=200>
</br> 
### Rear Illuminated
<img src="../SAOuija/images/backlitFront.png" height=200> | <img src="../SAOuija/images/backlitBack.png" height=200>
<br>

## Bugs/Features:

| Frontlit | Backlit  |
| -------- | -------- |
| Power with 3.3V - 5.0 V |
| ATtiny1616 Processor| ATtiny816 Processor |
| I2C pins on SAO connectors have 10K pullup resistors|
| Bodge wire needed connect "0" LED to GPIO1  | No bodge |
| GPIO1 & GPIO2 not connected to SAO connector | GPIO1/GPIO2 both connected|
| Multiplexed LEDs (7 rows, 6 cols) connected directly to ATtiny pins | Multiplexed LEDs with 7 rows connectged to shift register, 6 cols connected to ATtiny pins |

## Using the buttons to set your own text:
Both boards have "SELECT" and "MODE" buttons. There are three different modes:
-  DISPLAY mode (default): 
    - A stored text string is displayed one letter at a time and the "SAOuija" LEDs light up briefly between iterations through the string. 
    - SAOuija automatically starts in DISPLAY mode.
    - When in DISPLAY mode, a long press on the "MODE" button triggers ENTRY mode.

-  ENTRY mode:
    - When in ENTRY MODE, one character (letter or number) is highlighted (selected) at a time, and the "SAOuija" LEDs display a "scanner" animation pattern.
    - When ENTRY mode first starts, a new text string may be formed by selecting a sequence of characters (letters & numbers) using the steps below.
    - Advance through highlighted letters/numbers on the board with *short* presses of the "SELECT" button. 
    - A long (> 600 ms) press of the "SELECT" button, adds the currently highlighted character to the new text string. You may add the same letter to the string by two long presses of "SELECT" in a row, without a short press in between.
    - When in ENTRY mode, a *short* press of the "MODE" button adds a space character to the text string
    - Anytime a new character is added to the text string being formed, the "scanner" animation stops briefly and all four "SAOuija" LEDs light up.
    - When in ENTRY mode, a *long* (> 600 ms) press on the "MODE" button triggers "ACCEPT" mode.
- ACCEPT mode:
    - In "ACCEPT" mode, either the "YES" or "NO" button will be illuminated at one time
    - Toggle between "YES" and "NO" using *short* presses of the select button
    - To keep the newly specified text once it is completely enterd, hold the "SELECT" button for a *long* press when "YES" is highlighted.
        - The newly specified text will be stored in the boards EEPROM
        - SAOuija will return to DISPLAY mode and show the new text message
    - To discard the newly specified text and return to displaying the previous text, you can:
        - Hold the "SELECT" button for a *long* press while "NO" is highlighted -or-
        - Hold the "MODE" button for a *long* press (the highlighted word doesn't matter)
  

## Uploading your own text via Arduino code:
TBD
