package org.androidaudioplugin.hosting

import org.androidaudioplugin.ParameterInformation
import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.Executor

class ParameterMetadataPublisherTest {
    private class QueueExecutor : Executor {
        val tasks = ArrayDeque<Runnable>()
        override fun execute(command: Runnable) { tasks.addLast(command) }
        fun drain() { while (tasks.isNotEmpty()) tasks.removeFirst().run() }
    }
    private fun snapshot(revision: Long, name: String = "Blank") = ParameterMetadataSnapshot.fromNative(
        revision, arrayOf(ParameterInformation(9, name, 0.0, 1.0, 0.5)))

    @Test fun lateSubscribersReplayAndBurstsCoalesceWithSameCountChanges() {
        val publisher = ParameterMetadataPublisher()
        val executor = QueueExecutor()
        val received = mutableListOf<ParameterMetadataSnapshot>()
        publisher.publish(snapshot(1))
        publisher.add(executor, true) { received.add(it) }
        publisher.publish(snapshot(2, "Muff Drive"))
        publisher.publish(snapshot(3, "Muff Tone"))
        assertEquals(1, executor.tasks.size)
        executor.drain()
        assertEquals(listOf(3L), received.map { it.revision })
        assertEquals("Muff Tone", received.single().parameters.single().name)
        assertEquals(3L, publisher.latest?.revision)
    }

    @Test fun closingOneSubscriberDoesNotCancelOthersOrAnExecutingCallback() {
        val publisher = ParameterMetadataPublisher()
        val executor = QueueExecutor()
        var firstCalls = 0
        var secondCalls = 0
        val first = publisher.add(executor, true) { firstCalls++ }
        publisher.add(executor, true) { secondCalls++ }
        publisher.publish(snapshot(1))
        first.close()
        first.close()
        executor.drain()
        publisher.publish(snapshot(2))
        executor.drain()
        assertEquals(0, firstCalls)
        assertEquals(2, secondCalls)
    }

    @Test fun closingInstanceSuppressesQueuedDeliveryAndRejectsSubscriptions() {
        val publisher = ParameterMetadataPublisher()
        val executor = QueueExecutor()
        var called = false
        publisher.add(executor, true) { called = true }
        publisher.publish(snapshot(1))
        publisher.close()
        publisher.publish(snapshot(2))
        executor.drain()
        assertFalse(called)
        assertEquals(1L, publisher.latest?.revision)
        assertThrows(IllegalStateException::class.java) { publisher.add(executor, true) {} }
    }

    @Test fun staleSnapshotsAndListenerFailuresDoNotInterruptAnotherListener() {
        val failures = mutableListOf<Throwable>()
        val publisher = ParameterMetadataPublisher(failures::add)
        val executor = QueueExecutor()
        val revisions = mutableListOf<Long>()
        publisher.add(executor, false) { error("Listener failed") }
        publisher.add(executor, false) { revisions.add(it.revision) }
        publisher.publish(snapshot(2))
        publisher.publish(snapshot(1))
        executor.drain()
        assertEquals(listOf(2L), revisions)
        assertEquals(1, failures.size)
    }

    @Test fun replayCanBeDisabledAndAnEmptyLayoutIsAPublication() {
        val publisher = ParameterMetadataPublisher()
        val executor = QueueExecutor()
        val received = mutableListOf<ParameterMetadataSnapshot>()
        publisher.publish(snapshot(1))
        publisher.add(executor, false) { received.add(it) }
        executor.drain()
        assertTrue(received.isEmpty())
        publisher.publish(ParameterMetadataSnapshot.fromNative(2, emptyArray()))
        executor.drain()
        assertTrue(received.single().parameters.isEmpty())
    }

    @Test fun snapshotsOwnTheirMetadataAndCompatibilityConversionsAreIndependent() {
        val source = ParameterInformation(9, "Drive", 0.0, 1.0, 0.5)
        source.enumerations.add(ParameterInformation.EnumerationInformation(0, 0.0, "Off"))
        val snapshot = ParameterMetadataSnapshot.fromNative(1, arrayOf(source))
        source.name = "Changed"
        source.enumerations[0].name = "Changed"
        val copy = snapshot.parameters.single().toParameterInformation()
        copy.name = "Also changed"
        copy.enumerations.clear()
        assertEquals("Drive", snapshot.parameters.single().name)
        assertEquals("Off", snapshot.parameters.single().enumerations.single().name)
        assertThrows(UnsupportedOperationException::class.java) {
            (snapshot.parameters as MutableList).clear()
        }
    }
}
