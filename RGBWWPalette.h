typedef struct PalettePoint {
  union {
    struct {
      uint8_t b, g, r, w;
    };
    uint32_t wrgb;
  };
  uint8_t pos;
};


uint32_t interpolatergbw(uint32_t a, uint32_t b, int level) { // Byte order is 0XWWBBRRGG, but doesnt really matter in the long run anyway
  level = min(max(0, level), 255);
  //Linear Interpolation between two RGBW colors A and B, a level of 0 is A, a level of 255 is B
  uint32_t result = 0;
  uint8_t *result_pointer = (uint8_t *) & result;
  uint8_t *apointer = (uint8_t *) & a;
  uint8_t *bpointer = (uint8_t *) & b;
  for (byte i = 0; i < 4; i ++) result_pointer[i] = (uint8_t) (((255 - level) * apointer[i] + level * bpointer[i] ) >> 8) ;
  return result;
}


uint32_t getFromPalette(struct PalettePoint *pp, uint8_t palettesize, uint8_t level) {
  if (palettesize == 1) return pp[0].wrgb; //handle single color palettes
  for (uint8_t i = 0; i < palettesize; i++) {
    if (pp[i].pos >= level) {
      //Serial.print(pp[i].pos);
      //Serial.print("pos level");
      //Serial.println(level);
      if (i == 0) return pp[0].wrgb; //no interpolation if we match before first color
      int newlevel = (255 * (level - pp[i - 1].pos)) / (pp[i].pos - pp[i - 1].pos);
      return interpolatergbw(pp[i - 1].wrgb, pp[i].wrgb, newlevel);
    }
  }
  return pp[palettesize].wrgb; //if we dont match any levels, return last color in palette
}

typedef struct ColorBGRW {
  union {
    uint32_t wrgb;
    struct {
      uint8_t b, g, r, w;
    };
    uint8_t raw[4];
  };
};


void printColorWRGB(uint32_t color) {
  uint8_t *colorpointer = (uint8_t *) & color;

  Serial.print('R');
  Serial.print(colorpointer[2]);
  Serial.print(" G");
  Serial.print(colorpointer[1]);
  Serial.print(" B");
  Serial.print(colorpointer[0]);
  Serial.print(" W");
  Serial.println(colorpointer[3]);

}