/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL)
	{
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL)
	{
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL)
	{
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock); /*lo spinlock protegge il contatore del semaforo*/

	while (sem->sem_count == 0)
	{
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0); /*se quest'espressione è falsa allora chiama panic*/
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

/*LAB3: devo completare/cambiare le funzioni lock_create(), lock_destroy(), lock_acquire(), lock_release(), lock_do_i_hold()*/
/*************************************************SEMAFORO***************************/
struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock)); /*alloca la struct lock*/
	if (lock == NULL)
	{
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL)
	{
		kfree(lock);
		return NULL;
	}

#if SEM
	/*LAB3: creo il semaforo del lock (versione con semaforo)*/
	lock->lock_sem = sem_create(lock->lk_name, 1);
	if (lock->lock_sem == NULL)
	{
#else
	/*inizializzo spinlock e wait channel*/
	lock->lock_wchan = wchan_create(lock->lk_name);
	if (lock->lock_wchan == NULL)
	{
#endif
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}
	lock->thread_who_acquired = NULL;
	spinlock_init(&lock->lock_spinlock);
	return lock;
}

void lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);

	/*LAB3: distruggo anche il semaforo del lock e libero thread_who_acquired*/
	spinlock_cleanup(&lock->lock_spinlock);
#if SEM
	sem_destroy(lock->lock_sem);
#else
	wchan_destroy(lock->lock_wchan);

#endif

	kfree(lock->lk_name);
	kfree(lock);
}

/*si mette void per evitare che il compilatore dia dei warning compilati come errori legati al fatto di avere un parametro o una variabile che sono dichiarati ma non utilizzati --> si scrive un'espressione con un solo dato, si fa il cast a void per dire che non la assegno a nessuna variabile*/
void lock_acquire(struct lock *lock)
{

	KASSERT(lock != NULL);
	KASSERT(!(lock_do_i_hold(lock)));			 /*fermati se possiedo già il lock*/
	KASSERT(curthread->t_in_interrupt == false); /*fermati se sono in interruzione*/

#if SEM
	/*LAB3: implemento lock acquire utilizzando P(sem)*/
	P(lock->lock_sem); //versione con semaforo
	/*siccome alla fine della P rilascio lo spinlock, lo riacquisisco subito dopo*/
	spinlock_acquire(&lock->lock_spinlock);
#else
	/*siccome per usare la wchan devo possedere lo spinlock, devo prima acquisirlo*/
	spinlock_acquire(&lock->lock_spinlock);
	while (lock->thread_who_acquired != NULL)
	{
		/*dormi finchè qualcuno non rilascia il lock*/
		wchan_sleep(lock->lock_wchan, &lock->lock_spinlock);
	}
#endif
	KASSERT(lock->thread_who_acquired == NULL); /*fermati se il thread che ha acquisito il lock non è null*/
	lock->thread_who_acquired = curthread;
	spinlock_release(&lock->lock_spinlock);
}

void lock_release(struct lock *lock)
{

	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock)); /*bloccati se non c'è ownership*/
	spinlock_acquire(&lock->lock_spinlock);
	/*prima di fare wakeone devo acquisire lo spinlock*/
	lock->thread_who_acquired = NULL;
#if SEM
	V(lock->lock_sem); //versione con semaforo
#else
	wchan_wakeone(lock->lock_wchan, &lock->lock_spinlock);
#endif
	spinlock_release(&lock->lock_spinlock);
}
bool lock_do_i_hold(struct lock *lock)
{
	/*funzione che ritorna vero se il thread possiede il lock*/
	// Write this

	bool result;
#if SEM
	spinlock_acquire(&lock->lock_spinlock);
	result = lock->thread_who_acquired == curthread;
	spinlock_release(&lock->lock_spinlock);
#else
	result = lock->thread_who_acquired == curthread;
#endif
	return result;
}

////////////////////////////////////////////////////////////
//
// CV

struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL)
	{
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name == NULL)
	{
		kfree(cv);
		return NULL;
	}

	/*lab03: sync*/
	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL)
	{
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	spinlock_init(&cv->cv_spinlock);
	return cv;
}

void cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	spinlock_cleanup(&cv->cv_spinlock);
	wchan_destroy(cv->cv_wchan);
	kfree(cv->cv_name);
	kfree(cv);
}

/*atomic operation*/
void cv_wait(struct cv *cv, struct lock *lock)
{

	KASSERT(lock != NULL);
	KASSERT(cv != NULL);
	KASSERT(lock_do_i_hold(lock));

	spinlock_acquire(&cv->cv_spinlock);

	lock_release(lock);
	wchan_sleep(cv->cv_wchan, &cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);

	lock_acquire(lock);
}

/*operazione atomica*/
void cv_signal(struct cv *cv, struct lock *lock)
{

	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeone(cv->cv_wchan, &cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);

}

void cv_broadcast(struct cv *cv, struct lock *lock)
{

	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cv_spinlock);
	wchan_wakeall(cv->cv_wchan, &cv->cv_spinlock);
	spinlock_release(&cv->cv_spinlock);

}
