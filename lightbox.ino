#include <Adafruit_NeoPixel.h>
#define PIN 5
#define LED_COUNT (uint8_t)60
#define STEP_DELAY (uint8_t)200

#include <EEPROM.h>

#include <SoftwareSerial.h>
SoftwareSerial BTserial(2, 3); // RX | TX

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, PIN,
NEO_GRB + NEO_KHZ800);

//for rainbow modes - reinitialized on start
int rainbowCycleIteration = 256 * 5;
int rainbowIteration = 256;
//for moving gradient
int gradientStepCount = 1;
int gradientStep = 1;
bool forwardGradient = true; //to avoid swapping arrays

char mode;

//current colors
byte alphaArrayStart[LED_COUNT ];
byte redArrayStart[LED_COUNT ];
byte greenArrayStart[LED_COUNT ];
byte blueArrayStart[LED_COUNT ];

//target colors
byte alphaArrayTarget[LED_COUNT ];
byte redArrayTarget[LED_COUNT ];
byte greenArrayTarget[LED_COUNT ];
byte blueArrayTarget[LED_COUNT ];

//time before switch off
long timeToSwitchOff = 0;
bool switchedOff = false;

void setup() {
	BTserial.setTimeout(50);
	BTserial.begin(38400);
	strip.setBrightness(EEPROM[490]);
	strip.begin();
	strip.show(); // Initialize all pixels to 'off'

	mode = EEPROM[0];
	switch (mode) {
	case 'm':
		//init stripe from EEPROM
		loadStripeState();
		break;
	case 'v':
		//init stripe from EEPROM + init additional vars for cycle
		loadStripeState();
		loadGradientState();
		break;
	case 'o':
	case 'c':
		//rainbow modes, will be inited in cycle
		break;
	default:
		break;
	}

}

void loop() {
	long startTime = millis();
	//try to read data from BT
	while (BTserial.available() > 0) {
		BTserial.flush();
		char tempMode = BTserial.read();
		switch (tempMode) {
		case '?':
			BTserial.read();
			status();
			break;
		case 'r':
			BTserial.read();
			reset(BTserial.parseInt());
			mode = tempMode;
			switchedOff = false;
			timeToSwitchOff = 0;
			loadStripeState();
			break;
			//Just switch to rainbow modes, nothing else to do
		case 'o':
		case 'c':
			BTserial.read();
			mode = tempMode;
			EEPROM[0] = mode;
			switchedOff = false;
			timeToSwitchOff = 0;
			break;
			//single color
		case 's':
			switchedOff = false;
			timeToSwitchOff = 0;

			{
				byte secondColor = BTserial.parseInt();
				byte shift = 0;
				if (secondColor == 1) {
					mode = 'v';
					EEPROM[244 + 1] = BTserial.parseInt();
					shift = 245;
				} else {
					mode = 'm';
				}
				byte startingLed = BTserial.parseInt();
				byte ledCount = BTserial.parseInt();
				EEPROM[0] = mode;
				byte alpha = BTserial.parseInt();
				byte red = BTserial.parseInt();
				byte green = BTserial.parseInt();
				byte blue = BTserial.parseInt();
				for (int i = startingLed; i < startingLed + ledCount ; i++) {
					EEPROM[shift + 1 + i * 4] = alpha;
					EEPROM[shift + 2 + i * 4] = red;
					EEPROM[shift + 3 + i * 4] = green;
					EEPROM[shift + 4 + i * 4] = blue;
				}

				BTserial.read();
				if (secondColor == 1) {
					loadGradientState();
				} else {
					loadStripeState();
				}
			}
			break;
			//gradient staring point + number + color1 + color2
		case 'g':
			switchedOff = false;
			timeToSwitchOff = 0;
			{
				byte secondColor = BTserial.parseInt();
				byte shift = 0;
				if (secondColor == 1) {
					mode = 'v';
					EEPROM[244 + 1] = BTserial.parseInt();
					shift = 245;
				} else {
					mode = 'm';
				}
				EEPROM[0] = mode;
				byte gradientStartLed = BTserial.parseInt();
				byte gradiendLedCount = BTserial.parseInt();
				byte alpha = BTserial.parseInt();
				byte red = BTserial.parseInt();
				byte green = BTserial.parseInt();
				byte blue = BTserial.parseInt();
				byte alpha2 = BTserial.parseInt();
				byte red2 = BTserial.parseInt();
				byte green2 = BTserial.parseInt();
				byte blue2 = BTserial.parseInt();
				byte steps = (LED_COUNT - gradiendLedCount * 2) / 2;
				//don't work for 0!!!! steps
				for (int i = 0; i <= steps + 1; i++) {

					byte nextLedIndex = gradientStartLed + gradiendLedCount + i
							- 1;
					if (nextLedIndex >= LED_COUNT) {
						nextLedIndex = nextLedIndex - LED_COUNT;
					}
					int prevLedIndex = gradientStartLed - i;
					if (prevLedIndex < 0) {
						prevLedIndex = prevLedIndex + LED_COUNT;
					}
					int alphaDiff = getGradientComponent(alpha, alpha2, i,
							steps + 1);
					int redDiff = getGradientComponent(red, red2, i, steps + 1);
					int blueDiff = getGradientComponent(blue, blue2, i,
							steps + 1);
					int greenDiff = getGradientComponent(green, green2, i,
							steps + 1);
					if (i == 0 || i == steps + 1) {
						bool hasGap = (i == 0 && prevLedIndex > nextLedIndex)
								|| (i == steps + 1
										&& nextLedIndex > prevLedIndex);
						if (hasGap) {
							for (int j = max(prevLedIndex, nextLedIndex);
									j < LED_COUNT ; j++) {
								EEPROM[shift + 1 + j * 4] = alpha + alphaDiff;
								EEPROM[shift + 2 + j * 4] = red + redDiff;
								EEPROM[shift + 3 + j * 4] = green + greenDiff;
								EEPROM[shift + 4 + j * 4] = blue + blueDiff;

							}
							for (int j = 0;
									j <= min(prevLedIndex, nextLedIndex); j++) {
								EEPROM[shift + 1 + j * 4] = alpha + alphaDiff;
								EEPROM[shift + 2 + j * 4] = red + redDiff;
								EEPROM[shift + 3 + j * 4] = green + greenDiff;
								EEPROM[shift + 4 + j * 4] = blue + blueDiff;

							}
						} else {
							for (int j = min(prevLedIndex, nextLedIndex);
									j <= max(prevLedIndex, nextLedIndex); j++) {
								EEPROM[shift + 1 + j * 4] = alpha + alphaDiff;
								EEPROM[shift + 2 + j * 4] = red + redDiff;
								EEPROM[shift + 3 + j * 4] = green + greenDiff;
								EEPROM[shift + 4 + j * 4] = blue + blueDiff;
							}
						}
					} else {
						EEPROM[shift + 1 + nextLedIndex * 4] = alpha
								+ alphaDiff;
						EEPROM[shift + 2 + nextLedIndex * 4] = red + redDiff;
						EEPROM[shift + 3 + nextLedIndex * 4] = green
								+ greenDiff;
						EEPROM[shift + 4 + nextLedIndex * 4] = blue + blueDiff;
						EEPROM[shift + 1 + prevLedIndex * 4] = alpha
								+ alphaDiff;
						EEPROM[shift + 2 + prevLedIndex * 4] = red + redDiff;
						EEPROM[shift + 3 + prevLedIndex * 4] = green
								+ greenDiff;
						EEPROM[shift + 4 + prevLedIndex * 4] = blue + blueDiff;
					}

				}

				BTserial.read();
				if (secondColor == 1) {
					loadGradientState();
				} else {
					loadStripeState();
				}
				break;
			}
			//reset sleep mode
		case 'e':
			switchedOff = false;
			timeToSwitchOff = 0;
			if (mode == 'm') {
				loadStripeState();
			}
			if (mode == 'v') {
				loadStripeState();
				loadGradientState();
			}
			break;
			//switch off mode
		case 'w':
			BTserial.read();
			timeToSwitchOff = BTserial.parseInt();
			break;
		default:
			break;
		}
	}
	if (!switchedOff) {
		//dynamic modes
		switch (mode) {
		case 'v':
			gradientStep = gradient(gradientStep);
			break;
		case 'o':
			rainbowIteration = rainbow(rainbowIteration);
			break;
		case 'c':
			rainbowCycleIteration = rainbowCycle(rainbowCycleIteration);
			break;
		default:
			break;
		}
	}
//	delay(50);
	long executionTime = millis() - startTime;
	if (timeToSwitchOff > 0) {
		timeToSwitchOff -= executionTime;
		if (timeToSwitchOff <= 0) {
			switchedOff = true;
			for (int i = 0; i < LED_COUNT ; i++) {
				strip.setPixelColor(i, strip.Color(0, 0, 0));
			}
			strip.show();
		}
	}
}

void loadStripeState() {
	for (int i = 0; i < LED_COUNT ; i++) {
		alphaArrayStart[i] = EEPROM[1 + i * 4];
		redArrayStart[i] = EEPROM[2 + i * 4];
		greenArrayStart[i] = EEPROM[3 + i * 4];
		blueArrayStart[i] = EEPROM[4 + i * 4];
		alphaArrayTarget[i] = 0;
		redArrayTarget[i] = 0;
		greenArrayTarget[i] = 0;
		blueArrayTarget[i] = 0;
		float ratio = (float) alphaArrayStart[i] / 255.0;
		uint32_t color = strip.Color((int) (redArrayStart[i] * ratio),
				(int) (greenArrayStart[i] * ratio),
				(int) (blueArrayStart[i] * ratio));
		strip.setPixelColor(i, color);
	}
	strip.show();
}

void loadGradientState() {
	forwardGradient = true;
	gradientStep = 1;
	gradientStepCount = EEPROM[244 + 1] * 10;
	for (int i = 0; i < LED_COUNT ; i++) {
		alphaArrayTarget[i] = EEPROM[244 + 2 + i * 4];
		redArrayTarget[i] = EEPROM[244 + 3 + i * 4];
		greenArrayTarget[i] = EEPROM[244 + 4 + i * 4];
		blueArrayTarget[i] = EEPROM[244 + 5 + i * 4];
	}
}

void reset(byte newBrigthness) {
	EEPROM[0] = 'r';
	for (uint16_t i = 1; i < EEPROM.length(); i++) {
		EEPROM[i] = 0;
	}
	EEPROM[490] = newBrigthness;
    strip.setBrightness(newBrigthness);
	for (int i = 0; i < LED_COUNT ; i++) {
		alphaArrayStart[i] = 0;
		redArrayStart[i] = 0;
		greenArrayStart[i] = 0;
		blueArrayStart[i] = 0;
		alphaArrayTarget[i] = 0;
		redArrayTarget[i] = 0;
		greenArrayTarget[i] = 0;
		blueArrayTarget[i] = 0;
	}

}

void status() {
	//send lightbox parameters:
	//model: 0 - cave
	//start corner: 0 - left-bottom, 1 - left-top, 2 - right-top, 3 - right-bottom
	//0 - clockwise, 1 - counterclockwise
	//all pixels count
	//left pixels count
	//top pixels count
	//right pixels count
	//bottom pixels count
	//shift - basically number of starting led
	//mode
	//command (optional)
	String command = String("");
	String tempParams = String("");
	tempParams += String(gradientStepCount) + " " + String(timeToSwitchOff)
			+ " " + String(switchedOff ? 1 : 0);
	BTserial.print("Info: 0 0 1 ");
	BTserial.print(String(LED_COUNT));
	BTserial.print(" 18 12 18 12 1 ");
	BTserial.print(String(mode));
	BTserial.print(" " + tempParams);
	switch (mode) {
	case 'm':
	case 'v':
		for (int i = 0; i < LED_COUNT ; i++) {
			BTserial.print(" " + String(alphaArrayStart[i]));
			BTserial.print(" " + String(redArrayStart[i]));
			BTserial.print(" " + String(greenArrayStart[i]));
			BTserial.print(" " + String(blueArrayStart[i]));
		}

		for (int i = 0; i < LED_COUNT ; i++) {
			BTserial.print(" " + String(alphaArrayTarget[i]));
			BTserial.print(" " + String(redArrayTarget[i]));
			BTserial.print(" " + String(greenArrayTarget[i]));
			BTserial.print(" " + String(blueArrayTarget[i]));
		}
		break;

	default:
		break;

	}
	BTserial.print("\n");
	BTserial.flush();

}

int gradient(int gradientStep) {
	if (gradientStep <= gradientStepCount && gradientStep > 0) {
		for (uint16_t i = 0; i < strip.numPixels(); i++) {
			int diffA = 0;
			int diffR = 0;
			int diffG = 0;
			int diffB = 0;
			diffA = (alphaArrayTarget[i] - alphaArrayStart[i]) * gradientStep
					/ gradientStepCount;
			diffR = (redArrayTarget[i] - redArrayStart[i]) * gradientStep
					/ gradientStepCount;
			diffG = (greenArrayTarget[i] - greenArrayStart[i]) * gradientStep
					/ gradientStepCount;
			diffB = (blueArrayTarget[i] - blueArrayStart[i]) * gradientStep
					/ gradientStepCount;

			float ratio = (float) (alphaArrayStart[i] + diffA) / 255.0;
			uint32_t color = strip.Color(
					(int) ((redArrayStart[i] + diffR) * ratio),
					(int) ((greenArrayStart[i] + diffG) * ratio),
					(int) ((blueArrayStart[i] + diffB) * ratio));
			strip.setPixelColor(i, color);
		}
		strip.show();
		delay(STEP_DELAY);
	} else {
		forwardGradient = !forwardGradient;
	}
	if (forwardGradient) {
		return gradientStep + 1;
	} else {
		return gradientStep - 1;

	}
}

int getGradientComponent(byte start, byte target, byte step, byte stepCount) {
	return (target - start) * step / stepCount;
}

int rainbow(int rainbowIteration) {
	if (rainbowIteration > 0) {
		for (uint16_t i = 0; i < strip.numPixels(); i++) {
			strip.setPixelColor(i, wheel((i + rainbowIteration) & 255));
		}
		strip.show();
		rainbowIteration = rainbowIteration - 1;
		delay(STEP_DELAY);
	} else {
		rainbowIteration = 256;
	}
	return rainbowIteration;
}

int rainbowCycle(int rainbowCycleIteration) {
	if (rainbowCycleIteration > 0) {
		for (uint16_t i = 0; i < strip.numPixels(); i++) {
			strip.setPixelColor(i,
					wheel(
							((i * 256 / strip.numPixels())
									+ rainbowCycleIteration) & 255));
		}
		strip.show();
		rainbowCycleIteration = rainbowCycleIteration - 1;
		delay(STEP_DELAY);
	} else {
		rainbowCycleIteration = 256 * 5;
	}
	return rainbowCycleIteration;
}

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t wheel(byte wheelPos) {
	wheelPos = 255 - wheelPos;
	if (wheelPos < 85) {
		return strip.Color(255 - wheelPos * 3, 0, wheelPos * 3);
	}
	if (wheelPos < 170) {
		wheelPos -= 85;
		return strip.Color(0, wheelPos * 3, 255 - wheelPos * 3);
	}
	wheelPos -= 170;
	return strip.Color(wheelPos * 3, 255 - wheelPos * 3, 0);
}
