/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Pool
 * @brief Pool-capacity functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pool_internal.h"
#include "ut_pool.h"

/**
 * Ensures that the pool is large enough for newCapacity elements.
 * This has the side effect of setting the POOL_NEVER_FREE_PUDDLES flag.
 * Without this, the pool could shrink back down to its original size.
 * Note that this does not take into account the number of elements already
 * used in the pool.
 *
 * @param[in] aPool The pool
 * @param[in] newCapacity The desired new-size of the pool
 *
 * @return 0 on success
 * @return -1 on failure
 *
 */
uintptr_t
pool_ensureCapacity(OMRPool *aPool, uintptr_t newCapacity)
{
	uintptr_t numElements;
	uintptr_t result = 0;

	Trc_pool_ensureCapacity_Entry(aPool, newCapacity);

	numElements = pool_capacity(aPool);

	/* mark each pool as POOL_NEVER_FREE_PUDDLES */
	aPool->flags |= POOL_NEVER_FREE_PUDDLES;

	if (newCapacity > numElements) {
		OMRPoolPuddleList *puddleList;
		OMRPoolPuddle *newPuddle, *lastPuddle;
		uintptr_t newSize = newCapacity - numElements;

		puddleList = OMRPOOL_PUDDLELIST(aPool);
		lastPuddle = OMRPOOLPUDDLELIST_NEXTPUDDLE(puddleList);
		for (;;) {
			if (lastPuddle->nextPuddle == 0) {
				break;
			}
			lastPuddle = OMRPOOLPUDDLE_NEXTPUDDLE(lastPuddle);
		}
		while (newSize > 0) {
			OMRPoolPuddle *puddle;

			if (newSize < aPool->elementsPerPuddle) {
				newSize = aPool->elementsPerPuddle;
			}

			newPuddle = poolPuddle_new(aPool);
			if (newPuddle == 0) {
				Trc_pool_ensureCapacity_OutOfMemory(newCapacity);
				result = -1;
			}

			/* Stick it at the end of the list. */
			NNWSRP_SET(lastPuddle->nextPuddle, newPuddle);
			NNWSRP_SET(newPuddle->prevPuddle, lastPuddle);
			/* And also at the top of the available puddle list. */
			puddle = WSRP_GET(puddleList->nextAvailablePuddle, OMRPoolPuddle *);
			if (puddle) {
				NNWSRP_SET(newPuddle->nextAvailablePuddle, puddle);
			}
			NNWSRP_SET(puddleList->nextAvailablePuddle, newPuddle);

			lastPuddle = newPuddle;
			newSize -= aPool->elementsPerPuddle;
		}
	}

	Trc_pool_ensureCapacity_Exit(result);
	return result;
}


/**
 * Returns the total capacity of a pool
 *
 * @param[in] aPool The pool
 *
 * @return 0 on error
 * @return numElements in aPool otherwise
 *
 */
uintptr_t
pool_capacity(OMRPool *aPool)
{
	uintptr_t numElements = 0;

	Trc_pool_capacity_Entry(aPool);

	if (aPool) {
		OMRPoolPuddleList *puddleList = OMRPOOL_PUDDLELIST(aPool);
		OMRPoolPuddle *walk = OMRPOOLPUDDLELIST_NEXTPUDDLE(puddleList);

		while (walk) {
			numElements += aPool->elementsPerPuddle;
			walk = OMRPOOLPUDDLE_NEXTPUDDLE(walk);
		}
	}

	Trc_pool_capacity_Exit(numElements);

	return numElements;
}


