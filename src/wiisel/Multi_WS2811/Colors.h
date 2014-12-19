#include <mbed.h>

#ifndef __included_colors_h
#define __included_colors_h

/**
 * Converts the components of a color, as specified by the HSB
 * model, to an equivalent set of values for the default RGB model.
 * <p>
 * The <code>saturation</code> and <code>brightness</code> components
 * should be floating-point values between zero and one
 * (numbers in the range 0.0-1.0).  The <code>hue</code> component
 * can be any floating-point number.  The floor of this number is
 * subtracted from it to create a fraction between 0 and 1.  This
 * fractional number is then multiplied by 360 to produce the hue
 * angle in the HSB color model.
 * <p>
 * The integer that is returned by <code>HSBtoRGB</code> encodes the
 * value of a color in bits 0-23 of an integer value that is the same
 * format used by the method {@link #getRGB() <code>getRGB</code>}.
 * This integer can be supplied as an argument to the
 * <code>Color</code> constructor that takes a single integer argument.
 * @param     hue   the hue component of the color
 * @param     saturation   the saturation of the color
 * @param     brightness   the brightness of the color
 * @return    the RGB value of the color with the indicated hue,
 *                            saturation, and brightness.
 */
void HSBtoRGB(float hue, float saturation, float brightness, uint8_t *pr, uint8_t *pg, uint8_t *pb);

/**
 * Converts the components of a color, as specified by the default RGB
 * model, to an equivalent set of values for hue, saturation, and
 * brightness that are the three components of the HSB model.
 * <p>
 * If the <code>hsbvals</code> argument is <code>null</code>, then a
 * new array is allocated to return the result. Otherwise, the method
 * returns the array <code>hsbvals</code>, with the values put into
 * that array.
 * @param     r   the red component of the color
 * @param     g   the green component of the color
 * @param     b   the blue component of the color
 * @param     hsbvals  the array used to return the
 *                     three HSB values, or <code>null</code>
 * @return    an array of three elements containing the hue, saturation,
 *                     and brightness (in that order), of the color with
 *                     the indicated red, green, and blue components.
 */
float* RGBtoHSB(uint8_t r, uint8_t g, uint8_t b, float* hsbvals);

#endif
