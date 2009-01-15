/***************************************************************************
 *   Copyright © 2005, 2006, 2007, 2008 by Gabriele Svelto                 *
 *   gabriele.svelto@gmail.com                                             *
 *                                                                         *
 *   This file is part of Jelatine.                                        *
 *                                                                         *
 *   Jelatine is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Jelatine is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Jelatine.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

package jelatine;

import java.lang.Thread;

/**
 * Internal VM class for handling methods from java.lang.Thread
 */
public final class VMThread
{
    /**
     * This class cannot be instantiated, instances are created only by the JVM
     * in the start() method
     */

    private VMThread()
    {
        ;
    }

    /**
     * Implements java.lang.Thread.currentThread()
     * @returns A reference to the current thread
     */
    public native static Thread currentThread();

    /**
     * Implements java.lang.Thread.yield()
     */
    public native static void yield();

    /**
     * Implements java.lang.Thread.sleep()
     * @param ms The number of milliseconds
     */
    public native static void sleep(long ms) throws InterruptedException;

    /**
     * Implements java.lang.Thread.start()
     * @param thread The thread to be started
     * @returns A new VMThread
     */
    public native static VMThread start(Thread thread);

    /**
     * Implements java.lang.Thread.activeCount()
     * @returns The number of active threads in the system
     */
    public native static int activeCount();

    /**
     * Implements java.lang.Thread.join()
     */
    public native static void join(Thread thread) throws InterruptedException;

    /**
     * Implements java.lang.Thread.interrupt()
     */
    public native static void interrupt(Thread thread);
}
