//
//  main.c
//  rzoom
//
//  Created by Tim Sullivan on 29/10/15.
//  Copyright © 2015 Tim Sullivan. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <hiredis.h>
#include <wand/MagickWand.h>
#include "magick-helper.h"

/**
 E.g 300 * 265px
 = 76800 x 76800px image  (5898 Megapixles)
 
 imagine we have a 10,000x10,000 image created in image magick as our output region
 scale factor is 0.13020833333333334 * 256px
 
 therefore each tile will be approx 35x35 pixels
 */
//
float scaleFactor () {
    int tileRadius = 300;
    int tileSize = 256;
    int outputSize = 10000;
    float scaleFactor = outputSize / (float) (tileSize * tileRadius);
    return scaleFactor;
}

float scaleRange(float valueIn, float baseMin, float baseMax, float limitMin, float limitMax) {
    return ((limitMax - limitMin) * (valueIn - baseMin) / (baseMax - baseMin)) + limitMin;
}


int main(int argc, char **argv) {
    unsigned int j;
    redisContext *c;
    redisReply *reply, *tileReply;
    const char *hostname = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : 6379;
    MagickBooleanType status;
    MagickWand *wand, *masterWand;
    PixelWand *pixWand;
    DrawingWand *dwand;
    
    //analysis vars
    long tileSizeSum = 0;
    int tileCount = 0;
    int maxSize = 0;
    char biggestTile[30];
    FILE *statsFile = fopen("stats.csv", "w");
    
    fprintf(statsFile, "\"x_coord\", \"y_coord\", \"size\"\n");
    
    
    printf("connecting to redis server\n");
    struct timeval timeout = {1, 500000}; // 1.5 seconds
    c = redisConnectWithTimeout(hostname, port, timeout);
    if (c == NULL || c->err) {
        if (c) {
            printf("Connection error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }
    
    float scale;
    scale = scaleFactor();
    int size = (int) 256 * scale;
    printf("pixel size for each tile will be %dpx\n", size);
    
    /* init magick */
    MagickWandGenesis();
   
    
    //master wand for source image
    masterWand = NewMagickWand();
    pixWand = NewPixelWand();
    PixelSetColor(pixWand, "white");
    status = MagickNewImage(masterWand, 10000, 10000, pixWand);
    
    if (status == MagickFalse){
        fprintf(stderr, "Couldn't create mega output canvas wand\n");
        ThrowWandException(wand);
    }
    
    reply = redisCommand(c, "KEYS tile:main:1:*");
    printf("Tile Key count: %zd\n", reply->elements);
    
    for(j = 0; j < reply->elements; j++){
        tileReply = redisCommand(c, "HGET %s data", reply->element[j]->str);
        wand = NewMagickWand(); //scratch wand for each tile
        status = MagickReadImageBlob(wand, tileReply->str, (size_t) tileReply->len);

        
        if (status == MagickFalse){
            fprintf(stderr, "Tile at %s is corrupt! Skipping...\n", reply->element[j]->str);
            continue;
        }
        
        //sum size
        tileSizeSum += tileReply->len;
        tileCount += 1;
        if(tileReply->len > maxSize){
            maxSize = tileReply->len;
            strcpy(biggestTile, reply->element[j]->str);
        }

        status = MagickThumbnailImage(wand, size, size);
        if (status == MagickFalse){
            ThrowWandException(wand);
        }
        
        //split on redis key segments ":"
        char *keyParts = malloc(strlen(reply->element[j]->str) + 1);
        strcpy(keyParts, reply->element[j]->str);
        char *token = strtok(keyParts, ":");
        int x, y, counter;
        x = y = counter = 0;
        
        while( token != NULL ){
            if(counter == 3){
                x = atoi(token);
            } else if(counter == 4){
                y = atoi(token);
            }
            token = strtok(NULL, ":");
            counter++;
        }
        free(keyParts);
        
        int xOrigin = (x * 256);
        int yOrigin = (y * 256);
        int scaledX = scaleRange(xOrigin, -150 * 256, 150 * 256, 0, 10000);
        int scaledY = scaleRange(yOrigin, -150 * 256, 150 * 256, 0, 10000);

        dwand = NewDrawingWand();
        status = DrawComposite(dwand, AtopCompositeOp, scaledX, scaledY, size + 1, size + 1, wand);
        MagickDrawImage(masterWand, dwand);
        
        if (status == MagickFalse){
            fprintf(stderr, "Draw to mega canvas failed\n");
            ThrowWandException(wand);
        }
        
        //write to csv file size
        fprintf(statsFile, "%d, %d, %d\n", x, y, tileReply->len / 1000 );
        
        DestroyMagickWand(wand);
        DestroyDrawingWand(dwand);
        freeReplyObject(tileReply);
        

    }
    
    printf("Total tiles: %d Average size: %ld kb\n", tileCount, (tileSizeSum / tileCount) / 1000);
    printf("Largest tile %s @ %dkb\n", biggestTile, maxSize / 1000);
    printf("Done loading tiles. Writing out megafile.png...\n");
    
    status = MagickWriteImage(masterWand, "megafile.png");
    if (status == MagickFalse){
        fprintf(stderr, "megafile.png write failed\n");
        ThrowWandException(wand);
    } else {
        printf("Done!\n");
    }
    
    

    DestroyMagickWand(masterWand);
    DestroyPixelWand(pixWand);
    freeReplyObject(reply);
    /* Disconnects and frees the context */
    redisFree(c);
    
    
    MagickWandTerminus();
    fclose(statsFile);
    
    return EXIT_SUCCESS;
}






