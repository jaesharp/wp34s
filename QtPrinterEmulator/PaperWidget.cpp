/* This file is part of 34S.
 *
 * 34S is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 34S is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 34S.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PaperWidget.h"
#include "PrinterEmulator.h"
#include "font82240b.h"

#define FIRST_PRINTABLE_CHAR 32
#define ESCAPE_CHAR 27
#define END_OF_LINE 10
#define LINE_FEED 4

#define RESET_PRINTER 255
#define SELF_TEST 254
#define USE_EXPANDED_CHARACTERS 253
#define USE_NORMAL_CHARACTERS 252
#define START_UNDERLINING 251
#define STOP_UNDERLINING 250
#define USE_ECMA94 249
#define USE_ROMAN8 248
#define GRAPHICS_MAX 166

PaperWidget::PaperWidget()
: x(0), y(0), xOffset(0), lineCount(PAPER_INITIAL_LINES), zoom(1), pixmap(0), painter(0), lastIsEscape(false), ecma94(false), underlined(false), expanded(false), expectedGraphicsChars(0)
{
	setMinimumSize(PAPER_WIDTH+PAPER_HORIZONTAL_MARGIN, PAPER_INITIAL_LINES*LINE_HEIGHT+PAPER_VERTICAL_MARGIN);
}

QSize PaperWidget::sizeHint() const
{
	return minimumSize();
}

void PaperWidget::append(const QByteArray& aText)
{
	print(aText);
	printedText+=aText;
	update();
}

void PaperWidget::clear()
{
	printedText.clear();
	deletePixmap();
	update();
}

// We do not pass a reference as the underlying array can be changed during print to remove the first line
void PaperWidget::print(QByteArray aText)
{
	for (int charIndex = 0; charIndex < aText.size(); ++charIndex)
	{
		bool escapeFound=false;
		int c=aText.at(charIndex);
		if(expectedGraphicsChars>0)
		{
			expectedGraphicsChars--;
			processGraphics(c);
		}
		else if(lastIsEscape)
		{
			processEscape(c);
		}
		else
		{
			switch(c)
			{
			case END_OF_LINE:
			{
				endOfLine();
				break;
			}
			case LINE_FEED:
			{
				lineFeed();
				break;
			}
			case ESCAPE_CHAR:
			{
				escapeFound=true;
				break;
			}
			default:
			{
				processNormalChar(c);
				break;
			}
			}
		}
		lastIsEscape=escapeFound;
	}
	emit printed(toY(y+LINE_HEIGHT));
}

void PaperWidget::buildPixmap()
{
	QSize currentSize=size();
	zoom=qMax(1,currentSize.width()/(PAPER_WIDTH+PAPER_HORIZONTAL_MARGIN));
	xOffset=(currentSize.width()-PAPER_HORIZONTAL_MARGIN)%(PAPER_WIDTH*zoom);
	int newHeight=lineCount*LINE_HEIGHT*zoom;
	setMinimumHeight(newHeight);
	deletePixmap();
	// We rebuild the pixmap using the last lineCount before resetting it. This way, initial size should be ok.
	pixmap=new QPixmap(currentSize.width(), qMax(currentSize.height(), newHeight));
	pixmap->fill(Qt::white);
	painter=new QPainter(pixmap);
	painter->setPen(Qt::black);

	x=y=0;
	lineCount=0;

	print(printedText);
}

void PaperWidget::deletePixmap()
{
	if(painter!=0)
	{
		delete painter;
		painter=0;
	}
	if(pixmap!=0)
	{
		delete pixmap;
		pixmap=0;
	}
}

void PaperWidget::resizeEvent(QResizeEvent* aResizeEvent)
{
	Q_UNUSED(aResizeEvent)

	deletePixmap();
}

void PaperWidget::paintEvent(QPaintEvent* aPaintEvent)
{
	Q_UNUSED(aPaintEvent)

	if(pixmap==0)
	{
		buildPixmap();
	}

	QPainter paperPainter(this);
	paperPainter.drawPixmap(0, 0, *pixmap);
}

void PaperWidget::processEscape(int anEscapedChar)
{
	switch(anEscapedChar)
	{
	case RESET_PRINTER:
	{
		resetPrinter();
		break;
	}
	case SELF_TEST:
	{
		selfTest();
		break;
	}
	case USE_EXPANDED_CHARACTERS:
	{
		expanded=true;
		break;
	}
	case USE_NORMAL_CHARACTERS:
	{
		expanded=false;
		break;
	}
	case START_UNDERLINING:
	{
		underlined=true;
		break;
	}
	case STOP_UNDERLINING:
	{
		underlined=false;
		break;
	}
	case USE_ECMA94:
	{
		ecma94=true;
		break;
	}
	case USE_ROMAN8:
	{
		ecma94=false;
		break;
	}
	default:
	{
		if(anEscapedChar<=GRAPHICS_MAX)
		{
			expectedGraphicsChars=anEscapedChar;
		}
		break;
	}
	}
}

int PaperWidget::toX(int anX)
{
	return PAPER_HORIZONTAL_MARGIN/2+xOffset+anX*zoom;

}

int PaperWidget::toY(int anY)
{
	return PAPER_VERTICAL_MARGIN/2+anY*zoom;
}

void PaperWidget::lineFeed()
{
	if(lineCount>=MAX_LINES)
	{
		removeFirstLine();
	}
	y+=LINE_HEIGHT;
	int newHeight=toY(y+LINE_HEIGHT);
	if(newHeight>=pixmap->height())
	{
		QPixmap* newPixmap=new QPixmap(pixmap->width(), newHeight);
		newPixmap->fill(Qt::white);
		QPainter* newPainter=new QPainter(newPixmap);
		newPainter->setPen(Qt::black);
		newPainter->drawPixmap(0, 0, *pixmap);
		setMinimumHeight(newHeight);
		deletePixmap();
		pixmap=newPixmap;
		painter=newPainter;
	}
	x=0;
	lineCount++;
}

void PaperWidget::removeFirstLine()
{
	for (int charIndex = 0; charIndex < printedText.size(); ++charIndex)
	{
		int c=printedText.at(charIndex);
		if(c==LINE_FEED || c==END_OF_LINE)
		{
			printedText.remove(0, charIndex+1);
			break;
		}
	}

	QPixmap* newPixmap=new QPixmap(pixmap->size());
	newPixmap->fill(Qt::white);
	QPainter* newPainter=new QPainter(newPixmap);
	newPainter->setPen(Qt::black);
	int zoomedLineHeight=toY(LINE_HEIGHT);
	newPainter->drawPixmap(0, toY(0), *pixmap, 0, zoomedLineHeight, pixmap->width(), pixmap->height()-zoomedLineHeight);
	deletePixmap();
	pixmap=newPixmap;
	painter=newPainter;
	lineCount--;
	y-=LINE_HEIGHT;
}

void PaperWidget::endOfLine()
{
	lineFeed();
}

void PaperWidget::processNormalChar(int aChar)
{
	if(aChar>=FIRST_PRINTABLE_CHAR)
	{
		int expandedIncrement=expanded?2:1;
		x+=expandedIncrement;
		FONTDEF fontDef=(ecma94?sFontEcma94:sFontRoman8)[aChar-FIRST_PRINTABLE_CHAR];
		for(int charX=0; charX<HP82240B_CHARACTER_WIDTH; charX++)
		{
			unsigned char charColumn=fontDef.byCol[charX];
			unsigned char mask=0x1;
			for(int charY=0; charY<HP82240B_CHARACTER_HEIGHT; charY++, mask <<=1)
			{
				if((charColumn & mask)!=0)
				{
					drawHorizontalLine(x+charX, y+charY, expandedIncrement);
				}
			}
		}
		if(underlined)
		{
			drawHorizontalLine(x-expandedIncrement, y+HP82240B_CHARACTER_HEIGHT, (HP82240B_CHARACTER_WIDTH+2)*expandedIncrement);
		}
		x+=(HP82240B_CHARACTER_WIDTH+1)*expandedIncrement;
	}
}

void PaperWidget::drawPoint(int anX, int anY)
{
	painter->fillRect(toX(anX),toY(anY), zoom, zoom, Qt::black);
}

void PaperWidget::drawHorizontalLine(int anX, int anY, int aLength)
{
	painter->fillRect(toX(anX), toY(anY), aLength*zoom, zoom, Qt::black);
}

void PaperWidget::processGraphics(int aChar)
{
	unsigned char charColumn=(unsigned char) aChar;
	unsigned char mask=0x1;
	for(int charY=0; charY<HP82240B_CHARACTER_HEIGHT; charY++, mask <<=1)
	{
		if((charColumn & mask)!=0)
		{
			drawPoint(x, y+charY);
		}
	}
	++x;
}

void PaperWidget::resetPrinter()
{
    lastIsEscape=false;
    ecma94=false;
    underlined=false;
    expanded=false;
    expectedGraphicsChars=0;
}

void PaperWidget::selfTest()
{
}