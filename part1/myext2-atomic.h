#ifndef _ASM_GENERIC_BITOPS_MYEXT2_ATOMIC_H_
#define _ASM_GENERIC_BITOPS_MYEXT2_ATOMIC_H_

/*
 * Spinlock based version of myext2 atomic bitops
 */

#define myext2_set_bit_atomic(lock, nr, addr)		\
	({						\
		int ret;				\
		spin_lock(lock);			\
		ret = __test_and_set_bit_le(nr, addr);	\
		spin_unlock(lock);			\
		ret;					\
	})

#define myext2_clear_bit_atomic(lock, nr, addr)		\
	({						\
		int ret;				\
		spin_lock(lock);			\
		ret = __test_and_clear_bit_le(nr, addr);	\
		spin_unlock(lock);			\
		ret;					\
	})

#endif /* _ASM_GENERIC_BITOPS_MYEXT2_ATOMIC_H_ */
