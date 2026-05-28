#define STB_IMAGE_IMPLEMENTATION
#include "../libs/stb_image.h"
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <image_path>\n", argv[0]);
        return 1;
    }
    
    int width, height, channels;
    unsigned char* data = stbi_load(argv[1], &width, &height, &channels, 1);
    
    if (!data) {
        printf("Failed to load image: %s\n", argv[1]);
        return 1;
    }
    
    long long sum = 0;
    int total_pixels = width * height;
    
    for (int i = 0; i < total_pixels; ++i) {
        sum += data[i];
    }
    
    double avg = (double)sum / total_pixels;
    printf("Average metallic value: %.2f (0 is non-metal, 1 is full metal)\n", avg / 255.0);
    
    stbi_image_free(data);
    return 0;
}
