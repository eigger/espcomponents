#include "esphome.h"
#include "esphome/components/display/display.h"

class Point
{
public:
    Point() : Point(0, 0) {}
    Point(int x, int y) : X(x), Y(y) {}
    int X;
    int Y;
};

class Line
{
public:
    Point Point1;
    Point Point2;
};

void PrintUpTime(display::Display& it, int x, int y, BaseFont *font, Color color, TextAlign align, unsigned long up_time)
{
    int seconds = up_time / 1000;
    int minutes = up_time / 60;
    int hours = minutes / 60;
    int milliseconds = up_time % 1000;
    if (hours > 0)
    {
        it.printf(x, y, font, color, align, "%d:%02d:%02d", hours, minutes % 60, seconds % 60);
    }
    else if (minutes > 0)
    {
        it.printf(x, y, font, color, align, "%02d:%02d", minutes % 60, seconds % 60);
    }
    else if (seconds > 0)
    {
        it.printf(x, y, font, color, align, "%02d", seconds % 60);
    }
}

void PrintTemperature(display::Display& it, int x, int y, BaseFont *font, Color color, TextAlign align, float temperature)
{
    Color temperature_color = color;
    if (temperature < 1) temperature_color = Color(0, 0, 255);
    else if (temperature < 16) temperature_color = Color(0, 102, 255);
    else if (temperature < 25) temperature_color = Color(0, 255, 0);
    else if (temperature < 31) temperature_color = Color(255, 102, 0);
    else temperature_color = Color(255, 0, 0);
    it.printf(x, y, font, temperature_color, align, "%.1lf°", round(temperature * 10) / 10);
}

void PrintHumidity(display::Display& it, int x, int y, BaseFont *font, Color color, TextAlign align, float humidity)
{
    Color humidity_color = color;
    if (humidity < 31) humidity_color = Color(255, 0, 0);
    else if (humidity < 61) humidity_color = Color(0, 255, 0);
    else humidity_color = Color(0, 0, 255);
    it.printf(x, y, font, humidity_color, align, "%.0lf%%", round(humidity));
}

void PrintBatteryLevel(display::Display& it, int x, int y, BaseFont *font, Color color, TextAlign align, float level)
{
    it.printf(x, y, font, color, align, "%.0lf%%", round(level));
}

void PrintRSSI(display::Display& it, int x, int y, BaseImage *image, Color color, ImageAlign align, float rssi)
{
    Color rssi_color = color;
    if (rssi < -90) rssi_color = Color(255, 0, 0);
    else if (rssi < -67) rssi_color = Color(255, 153, 0);
    else if (rssi < -30) rssi_color = Color(255, 255, 0);
    else rssi_color = Color(0, 255, 0);
    it.image(x, y, image, align, rssi_color);
}

void PrintECO2(display::Display& it, int x, int y, BaseImage *image, Color color, ImageAlign align, float eco2)
{
    Color eco2_color = color;
    if (eco2 < 700) eco2_color = Color(0, 255, 0);
    else if (eco2 < 1000) eco2_color = Color(255, 255, 0);
    else if (eco2 < 2000) eco2_color = Color(255, 153, 0);
    else if (eco2 < 3000) eco2_color = Color(255, 102, 0);
    else eco2_color = Color(255, 0, 0);
    it.image(x, y, image, align, eco2_color);
}

void PrintECO2(display::Display& it, int x, int y, BaseFont *font, Color color, TextAlign align, float eco2)
{
    Color eco2_color = color;
    if (eco2 < 700) eco2_color = Color(0, 255, 0);
    else if (eco2 < 1000) eco2_color = Color(255, 255, 0);
    else if (eco2 < 2000) eco2_color = Color(255, 153, 0);
    else if (eco2 < 3000) eco2_color = Color(255, 102, 0);
    else eco2_color = Color(255, 0, 0);
    it.printf(x, y, font, eco2_color, align, "%.0lf", round(eco2));
}

void PrintTVOC(display::Display& it, int x, int y, BaseImage *image, Color color, ImageAlign align, float tvoc)
{
    Color tvoc_color = color;
    if (tvoc < 333) tvoc_color = Color(0, 255, 0);
    else if (tvoc < 1000) tvoc_color = Color(255, 255, 0);
    else if (tvoc < 3333) tvoc_color = Color(255, 153, 0);
    else if (tvoc < 8333) tvoc_color = Color(255, 102, 0);
    else tvoc_color = Color(255, 0, 0);
    it.image(x, y, image, align, tvoc_color);
}

void PrintTVOC(display::Display& it, int x, int y, BaseFont *font, Color color, TextAlign align, float tvoc)
{
    Color tvoc_color = color;
    if (tvoc < 333) tvoc_color = Color(0, 255, 0);
    else if (tvoc < 1000) tvoc_color = Color(255, 255, 0);
    else if (tvoc < 3333) tvoc_color = Color(255, 153, 0);
    else if (tvoc < 8333) tvoc_color = Color(255, 102, 0);
    else tvoc_color = Color(255, 0, 0);
    it.printf(x, y, font, tvoc_color, align, "%.0lf", round(tvoc));
}

Line GetRotatedLine(float angle, float radius)
{
    float radian = angle * (PI / 180);
    Line line;
    line.Point1.X = radius * cos(radian);
    line.Point2.X = radius * cos(radian) * -1;
    line.Point1.Y = radius * sin(radian);
    line.Point2.Y = radius * sin(radian) * -1;
    return line;
}

float MapValue(float value, float src_min, float src_max, float dst_min, float dst_max)
{
    return ((value - src_min) / (src_max - src_min) * (dst_max - dst_min)) + dst_min;
}

int GetYFromLine(Line line, int x)
{
    double m = (line.Point2.Y - line.Point1.Y) / (line.Point2.X - line.Point1.X);
    double b = line.Point1.Y - m * line.Point1.X;
    return x * m + b;
}

int GetXFromLine(Line line, int y)
{
    double m = (line.Point2.Y - line.Point1.Y) / (line.Point2.X - line.Point1.X);
    double b = line.Point1.Y - m * line.Point1.X;
    return (y - b) / m;
}

void PrintRollPitch(display::Display& it, int x, int y, BaseFont *font, Color color, TextAlign align, float roll_angle, float pitch_angle, float radius)
{
    float roll_radian = roll_angle * (PI / 180);
    float pitch_radian = pitch_angle * (PI / 180);
    Color roll_color = color;
    Color pitch_color = color;
    Color roll_pitch_color = color;

    if (roll_angle < 10) roll_color = Color(0, 255, 0);
    else if (roll_angle < 20) roll_color = Color(255, 153, 0);
    else roll_color = Color(255, 0, 0);

    if (pitch_angle < 10) pitch_color = Color(0, 255, 0);
    else if (pitch_angle < 20) pitch_color = Color(255, 153, 0);
    else pitch_color = Color(255, 0, 0);

    if (roll_angle < 10 && pitch_angle < 10) roll_pitch_color = Color(0, 255, 0);
    else if (roll_angle < 20 && pitch_angle < 20) roll_pitch_color = Color(255, 153, 0);
    else roll_pitch_color = Color(255, 0, 0);
    Line rotated_line = GetRotatedLine(roll_angle, radius * 3);

    float x_offset = MapValue(roll_angle, -90, 90, -radius, radius);
    float y_offset = MapValue(pitch_angle, -90, 90, -radius, radius);

    Color line_color = Color(30, 30, 30);
    it.line(x, y - radius, x, y + radius, line_color);
    it.line(x - radius, y, x + radius, y, line_color);
    it.line(x - radius, y + (radius / 2), x + radius, y + (radius / 2), line_color);
    it.line(x - radius, y - (radius / 2), x + radius, y - (radius / 2), line_color);
    //it.rectangle(leftX, topY, rightX - leftX, bottomY - topY, line_color);

    it.printf(x - radius - 7, y, font, roll_color, TextAlign::CENTER_RIGHT, "L");
    it.printf(x + radius + 7, y, font, roll_color, TextAlign::CENTER_LEFT, "R");
    it.printf(x + radius + 5, y + radius + 5, font, roll_color, TextAlign::TOP_CENTER, "%.0lf°", roll_angle);
    it.printf(x, y - radius - 5, font, pitch_color, TextAlign::BOTTOM_CENTER, "N");
    it.printf(x, y + radius + 5, font, pitch_color, TextAlign::TOP_CENTER, "T");
    it.printf(x - radius - 5, y + radius + 5, font, pitch_color, TextAlign::TOP_CENTER, "%.0lf°", pitch_angle);

    Line ref_line;
    Line adjust_line;
    ref_line.Point1.X = x + ref_line.Point1.X - x_offset;
    ref_line.Point2.X = x + ref_line.Point2.X - x_offset;
    ref_line.Point1.Y = y + ref_line.Point1.Y - y_offset;
    ref_line.Point2.Y = y + ref_line.Point2.Y - y_offset;
    adjust_line.Point1.X = x - radius;
    adjust_line.Point1.Y = GetYFromLine(ref_line, x - radius);
    adjust_line.Point2.X = x + radius;
    adjust_line.Point2.Y = GetYFromLine(ref_line, x + radius);

    it.line(adjust_line.Point1.X, adjust_line.Point1.Y, adjust_line.Point2.X, adjust_line.Point1.Y, roll_color);
    it.line(x, GetYFromLine(ref_line, x), x, y, pitch_color);
    it.filled_circle(adjust_line.Point1.X, adjust_line.Point1.Y, 2, roll_color);
    it.filled_circle(adjust_line.Point2.X, adjust_line.Point2.Y, 2, roll_color);
    it.filled_circle(x, y, 2, roll_pitch_color);
}

