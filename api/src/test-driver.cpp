/*
 * =====================================================================================
 *
 *       Filename:  test-driver.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/07/2011 01:48:23 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Cesar Prados Boda (cp), c.prados@gsi.de
 *        Company:  GSI
 *
 * =====================================================================================
 */

#include "fec.h"
#include <stdio.h>

int main()
{
    unsigned char msg[] =
        "HELLO WORLD!\n"
        "PLEASE SEND US MONEY!\n";

    int c;
    unsigned char* buf;
    const unsigned char* cbuf;
    unsigned int len;

    fec_open();

    for (c = 0; len = sizeof(msg), (buf = fec_encode(msg, &len, c)) != 0; ++c)
    {
        for (unsigned int x = 0; x < len; ++x)
            printf("%02x", buf[x]);
        printf("\n");
    }

    
    len = sizeof(msg);
    buf = fec_encode(msg, &len, 1);
    cbuf = fec_decode(buf, &len);
    printf("%x = 0\n", cbuf);

    len = sizeof(msg);
    buf = fec_encode(msg, &len, 1);
    cbuf = fec_decode(buf, &len);
    printf("%x = 0\n", cbuf);

    len = sizeof(msg);
    buf = fec_encode(msg, &len, 3);
    cbuf = fec_decode(buf, &len);
    printf("%x != 0\n", cbuf);

    if (cbuf != 0) {
        for (unsigned int i = 0; i < len; ++i)
            printf("%c", cbuf[i]);
        printf("---\n");
    }

    fec_close();
    return 0;
}
