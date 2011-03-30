#ifndef FEC_H
#define FEC_H

/* Initialize any buffers / state needed to encode/decode packets */
void fec_open();

/* Free any resources used by the encoder/decoder */
void fec_close();

/* Input: data received from ethernet payload [chunk, chunk+*len)
 * Output:
 *   If cannot (yet) decode, returns 0
 *   Otherwise, returns pointer to decoded message and modifies *len
 *
 * Note: inside be buffers
 */
unsigned char* fec_decode(unsigned char* chunk, unsigned int* len);

/* Input: ethernet payload to send [chunk, chunk+*len)
 *        index, starts at 0 and incremented each call until done
 * Output:
 *   If no more chunks follow, returns 0
 *   Returns encoded packet and modifies *len
 */
unsigned char* fec_encode(unsigned char* chunk, unsigned int* len, int index);

#endif