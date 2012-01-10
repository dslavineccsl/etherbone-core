/* Copyright (C) 2011-2012 GSI GmbH.
 *
 * Author: Wesley W. Terpstra <w.terpstra@gsi.de>
 *
 * This implements Etherbone memory using an system malloc/free.
 */

#ifndef EB_MEMORY_MALLOC_H
#define EB_MEMORY_MALLOC_H
#ifdef EB_USE_MALLOC

#define EB_CYCLE_OPERATION(x) (x)
#define EB_CYCLE(x) (x)

#define EB_NEW_FAILED 0

#endif
#endif
