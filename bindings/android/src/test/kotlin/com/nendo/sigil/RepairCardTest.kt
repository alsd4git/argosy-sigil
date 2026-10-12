// SPDX-License-Identifier: MPL-2.0
package com.nendo.sigil

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.BeforeClass
import org.junit.Test
import java.io.File
import java.nio.file.Files

/**
 * The card repair flow through JNI: collect refuses the Marvel vs. Capcom 2 VMU whose chain was
 * never written, repairCard rebuilds it, and collect reads the repaired card. Skips without the
 * host library, as [FileAccessTest] does, and without the sample.
 */
class RepairCardTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun hostLibrary() = FileAccessTest.hostLibrary()

        private val SAMPLE = File("../../tests/fixtures/saves/dc/files/mvc2-unwritten-fat-vmu/T1212N.A1.bin")
    }

    @Test
    fun repairsAnUnwrittenVmuChainAndCollectThenReadsIt() {
        assumeTrue("Dreamcast sample missing", SAMPLE.exists())
        val root = Files.createTempDirectory("sigil-repair").toFile()
        val card = File(root, "T1212N.A1.bin")
        SAMPLE.copyTo(card)
        val mvc2 = SigilResult.persisted("dc", "T1212N", "T1212N", 0)
        val options = mapOf("reicast_per_content_vmus" to "VMU A1")
        val refused = try {
            Sigil.collect(mvc2, "flycast", "Marvel vs. Capcom 2 (USA).gdi", root.path, options = options)
            null
        } catch (e: SigilException) {
            e
        }
        assertEquals(SigilException.DAMAGED, refused?.code)

        val repair = Sigil.repairCard(card.path)
        assertEquals(SigilCardListing.Format.DreamcastVmu, repair.format)
        assertEquals(1, repair.entries.size)
        val entry = repair.entries[0]
        assertEquals("MVLVSCP2_SYS", entry.name)
        assertEquals(199, entry.firstBlock)
        assertEquals(SigilCardRepairEntry.Damage.ChainUnwritten, entry.damage)
        assertTrue(entry.repaired)
        assertEquals(131072, repair.data?.size)

        card.writeBytes(repair.data!!)
        val unit = Sigil.collect(mvc2, "flycast", "Marvel vs. Capcom 2 (USA).gdi", root.path, options = options)
        assertNotNull(unit.data)
        root.deleteRecursively()
    }

    @Test
    fun aWholeCardHasNothingToRepair() {
        val gundam = File("../../tests/fixtures/saves/dc/files/gundam-0079-flycast").listFiles()?.firstOrNull()
        assumeTrue("Dreamcast sample missing", gundam != null)
        val repair = Sigil.repairCard(gundam!!.path)
        assertTrue(repair.entries.isEmpty())
        assertFalse(repair.data != null)
    }
}
