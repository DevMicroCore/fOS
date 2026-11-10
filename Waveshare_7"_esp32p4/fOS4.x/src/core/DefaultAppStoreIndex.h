#pragma once

#include <Arduino.h>

namespace FOSDefaultIndexes {

// Flash-only fallback for the public fOS store. The online index with the same
// format takes precedence, so new app releases do not require a firmware build.
static const char kAppStore[] PROGMEM = R"FOSIDX(FOS_APPSTORE_INDEX_V1
A	button_demo	1.0.0	2.0.0	Button Demo	457	button_demo/1.0.0/button_demo
F	button_demo	1.0.0	app.cfg	100
F	button_demo	1.0.0	layout.ui	357
A	calculator	1.0.0	2.0.0	Calculator	91	calculator/1.0.0/calculator
F	calculator	1.0.0	app.cfg	91
A	calculator	2.0.0	4.0.0	Calculator fScript	9682	calculator/2.0.0/calculator
F	calculator	2.0.0	README.md	825
F	calculator	2.0.0	app.json	253
F	calculator	2.0.0	layout.ui	2101
F	calculator	2.0.0	main.fapp	1954
F	calculator	2.0.0	main.fscript	4549
A	clock	1.0.0	2.0.0	Clock	82	clock/1.0.0/clock
F	clock	1.0.0	app.cfg	82
A	counter	1.0.0	4.0.0	fScript Counter	1359	counter/1.0.0/counter
F	counter	1.0.0	app.json	246
F	counter	1.0.0	layout.ui	338
F	counter	1.0.0	main.fapp	386
F	counter	1.0.0	main.fscript	389
A	drawing	1.0.0	4.0.0	fScript Zeichnen	2853	drawing/1.0.0/drawing
F	drawing	1.0.0	README.md	796
F	drawing	1.0.0	app.json	249
F	drawing	1.0.0	layout.ui	360
F	drawing	1.0.0	main.fapp	556
F	drawing	1.0.0	main.fscript	892
A	ebook	1.0.0	2.0.0	E-Book Demo	1063	ebook/1.0.0/ebook
F	ebook	1.0.0	app.cfg	102
F	ebook	1.0.0	book.txt	961
A	hello_fos	1.0.0	2.0.0	Hello fOS	483	hello_fos/1.0.0/hello_fos
F	hello_fos	1.0.0	app.cfg	85
F	hello_fos	1.0.0	layout.ui	398
A	radio	1.0.0	2.0.0	Radio	82	radio/1.0.0/radio
F	radio	1.0.0	app.cfg	82
A	text	1.0.0	2.0.0	Text	77	text/1.0.0/text
F	text	1.0.0	app.cfg	77
A	ui_demo	1.0.0	2.0.0	UI Demo	605	ui_demo/1.0.0/ui_demo
F	ui_demo	1.0.0	app.cfg	95
F	ui_demo	1.0.0	layout.ui	510
A	weather	1.0.0	2.0.0	Weather	88	weather/1.0.0/weather
F	weather	1.0.0	app.cfg	88
A	weather	2.0.0	4.0.0	Weather	16050	weather/2.0.0/weather
F	weather	2.0.0	app.json	254
F	weather	2.0.0	layout.ui	1350
F	weather	2.0.0	main.fapp	4953
F	weather	2.0.0	main.fscript	9493
A	button_demo	1.0.2	4.1.0	Button Demo	1030	button_demo/1.0.2/button_demo
F	button_demo	1.0.2	README.md	405
F	button_demo	1.0.2	app.cfg	96
F	button_demo	1.0.2	layout.ui	529
A	calculator	2.0.2	4.1.0	Calculator fScript	10641	calculator/2.0.2/calculator
F	calculator	2.0.2	README.md	1215
F	calculator	2.0.2	app.cfg	101
F	calculator	2.0.2	app.json	261
F	calculator	2.0.2	layout.ui	2561
F	calculator	2.0.2	main.fapp	1954
F	calculator	2.0.2	main.fscript	4549
A	counter	1.0.2	4.1.0	fScript Counter	2159	counter/1.0.2/counter
F	counter	1.0.2	README.md	409
F	counter	1.0.2	app.cfg	165
F	counter	1.0.2	app.json	254
F	counter	1.0.2	layout.ui	556
F	counter	1.0.2	main.fapp	386
F	counter	1.0.2	main.fscript	389
A	drawing	1.0.2	4.1.0	fScript Zeichnen	3627	drawing/1.0.2/drawing
F	drawing	1.0.2	README.md	1186
F	drawing	1.0.2	app.cfg	168
F	drawing	1.0.2	app.json	257
F	drawing	1.0.2	layout.ui	568
F	drawing	1.0.2	main.fapp	556
F	drawing	1.0.2	main.fscript	892
A	hello_fos	1.0.2	4.1.0	Hello fOS	1086	hello_fos/1.0.2/hello_fos
F	hello_fos	1.0.2	README.md	403
F	hello_fos	1.0.2	app.cfg	94
F	hello_fos	1.0.2	layout.ui	589
A	radio	2.0.2	4.1.0	Radio	7543	radio/2.0.2/radio
F	radio	2.0.2	README.md	1478
F	radio	2.0.2	app.cfg	105
F	radio	2.0.2	app.json	298
F	radio	2.0.2	layout.ui	1057
F	radio	2.0.2	main.fapp	1378
F	radio	2.0.2	main.fscript	3227
A	text	1.1.2	4.1.0	Text	13409	text/1.1.2/text
F	text	1.1.2	README.md	3670
F	text	1.1.2	app.cfg	104
F	text	1.1.2	app.json	294
F	text	1.1.2	layout.ui	2301
F	text	1.1.2	main.fapp	2356
F	text	1.1.2	main.fscript	4684
A	ui_demo	1.0.2	4.1.0	UI Demo	1089	ui_demo/1.0.2/ui_demo
F	ui_demo	1.0.2	README.md	401
F	ui_demo	1.0.2	app.cfg	91
F	ui_demo	1.0.2	layout.ui	597
A	weather	2.0.2	4.1.0	Weather	19797	weather/2.0.2/weather
F	weather	2.0.2	BAUANLEITUNG.md	2960
F	weather	2.0.2	README.md	409
F	weather	2.0.2	app.cfg	179
F	weather	2.0.2	app.json	266
F	weather	2.0.2	layout.ui	1537
F	weather	2.0.2	main.fapp	4953
F	weather	2.0.2	main.fscript	9493
)FOSIDX";

}  // namespace FOSDefaultIndexes
