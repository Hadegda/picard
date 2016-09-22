/*
 * The MIT License
 *
 * Copyright (c) 2015 The Broad Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

package picard.analysis;

import htsjdk.samtools.*;
import htsjdk.samtools.SamPairUtil.PairOrientation;
import htsjdk.samtools.metrics.MetricsFile;
import htsjdk.samtools.reference.ReferenceSequence;
import htsjdk.samtools.util.CoordMath;
import htsjdk.samtools.util.Histogram;
import htsjdk.samtools.util.SequenceUtil;
import htsjdk.samtools.util.StringUtil;
import picard.metrics.PerUnitMetricCollector;
import picard.metrics.SAMRecordAndReference;
import picard.metrics.SAMRecordAndReferenceMultiLevelCollector;

import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class AlignmentSummaryMetricsCollector extends SAMRecordAndReferenceMultiLevelCollector<AlignmentSummaryMetrics, Comparable<?>> {
    // If we have a reference sequence, collect metrics on how well we aligned to it
    private final boolean doRefMetrics;

    //Paired end reads above this insert size will be considered chimeric along with inter-chromosomal pairs.
    private final int maxInsertSize;

    //Paired-end reads that do not have this expected orientation will be considered chimeric.
    private final Set<PairOrientation> expectedOrientations;

    //Whether the SAM or BAM file consists of bisulfite sequenced reads.
    private final boolean isBisulfiteSequenced;

    //The minimum mapping quality a base has to meet in order to be considered high quality
    private final int MAPPING_QUALITY_THRESOLD = 20;

    //The minimum quality a base has to meet in order to be consider hq_20
    private final static int BASE_QUALITY_THRESHOLD = 20;

    //the adapter utility class
    private final AdapterUtility adapterUtility;

    public AlignmentSummaryMetricsCollector(final Set<MetricAccumulationLevel> accumulationLevels, final List<SAMReadGroupRecord> samRgRecords,
                                            final boolean doRefMetrics, final List<String> adapterSequence, final int maxInsertSize,
                                            final Set<PairOrientation> expectedOrientations, final boolean isBisulfiteSequenced) {
        this.doRefMetrics         = doRefMetrics;
        this.adapterUtility       = new AdapterUtility(adapterSequence);
        this.maxInsertSize        = maxInsertSize;
        this.expectedOrientations  = expectedOrientations;
        this.isBisulfiteSequenced = isBisulfiteSequenced;
        setup(accumulationLevels, samRgRecords);
    }

    @Override
    protected PerUnitMetricCollector<AlignmentSummaryMetrics, Comparable<?>, SAMRecordAndReference> makeChildCollector(String sample, String library, String readGroup) {
        return new GroupAlignmentSummaryMetricsPerUnitMetricCollector(sample, library, readGroup);
    }

    @Override
    public void acceptRecord(final SAMRecord rec, final ReferenceSequence ref) {
        if (!rec.isSecondaryOrSupplementary()) {
            super.acceptRecord(rec, ref);
        }
    }

    private class GroupAlignmentSummaryMetricsPerUnitMetricCollector implements PerUnitMetricCollector<AlignmentSummaryMetrics, Comparable<?>, SAMRecordAndReference> {
        final IndividualAlignmentSummaryMetricsCollector unpairedCollector;
        final IndividualAlignmentSummaryMetricsCollector firstOfPairCollector;
        final IndividualAlignmentSummaryMetricsCollector secondOfPairCollector;
        final IndividualAlignmentSummaryMetricsCollector pairCollector;
        final String sample;
        final String library;
        final String readGroup;

        public GroupAlignmentSummaryMetricsPerUnitMetricCollector(final String sample, final String library, final String readGroup) {
            this.sample = sample;
            this.library = library;
            this.readGroup = readGroup;
            unpairedCollector     = new IndividualAlignmentSummaryMetricsCollector(AlignmentSummaryMetrics.Category.UNPAIRED, sample, library, readGroup);
            firstOfPairCollector  = new IndividualAlignmentSummaryMetricsCollector(AlignmentSummaryMetrics.Category.FIRST_OF_PAIR, sample, library, readGroup);
            secondOfPairCollector = new IndividualAlignmentSummaryMetricsCollector(AlignmentSummaryMetrics.Category.SECOND_OF_PAIR, sample, library, readGroup);
            pairCollector         = new IndividualAlignmentSummaryMetricsCollector(AlignmentSummaryMetrics.Category.PAIR, sample, library, readGroup);
        }

        public void acceptRecord(final SAMRecordAndReference args) {
            final SAMRecord rec         = args.getSamRecord();
            final ReferenceSequence ref = args.getReferenceSequence();

            if (rec.getReadPairedFlag()) {
                if (rec.getFirstOfPairFlag()) {
                    firstOfPairCollector.addRecord(rec, ref);
                }
                else {
                    secondOfPairCollector.addRecord(rec, ref);
                }

                pairCollector.addRecord(rec, ref);
            }
            else {
                unpairedCollector.addRecord(rec, ref);
            }
        }

        @Override
        public void finish() {
            // Let the collectors do any summary computations etc.
            unpairedCollector.onComplete();
            firstOfPairCollector.onComplete();
            secondOfPairCollector.onComplete();
            pairCollector.onComplete();
        }

        @Override
        public void addMetricsToFile(final MetricsFile<AlignmentSummaryMetrics, Comparable<?>> file) {
            //if (firstOfPairCollector.getMetrics().TOTAL_READS > 0) {
            if (firstOfPairCollector.getMetrics().TOTAL_READS.get() > 0) {
                // override how bad cycle is determined for paired reads, it should be
                // the sum of first and second reads
                pairCollector.getMetrics().BAD_CYCLES = firstOfPairCollector.getMetrics().BAD_CYCLES +
                        secondOfPairCollector.getMetrics().BAD_CYCLES;

                file.addMetric(firstOfPairCollector.getMetrics());
                file.addMetric(secondOfPairCollector.getMetrics());
                file.addMetric(pairCollector.getMetrics());
            }

            //if there are no reads in any category then we will returned an unpaired alignment summary metric with all zero values
            //if (unpairedCollector.getMetrics().TOTAL_READS > 0 || firstOfPairCollector.getMetrics().TOTAL_READS == 0) {
            if (unpairedCollector.getMetrics().TOTAL_READS.get() > 0 || firstOfPairCollector.getMetrics().TOTAL_READS.get() == 0) {
                file.addMetric(unpairedCollector.getMetrics());
            }
        }

        /**
         * Class that counts reads that match various conditions
         */
        private class IndividualAlignmentSummaryMetricsCollector {
            private AtomicLong numPositiveStrand = new AtomicLong(0);
            private final Histogram<Integer> readLengthHistogram = new Histogram<Integer>();
            private AlignmentSummaryMetrics metrics;
            private AtomicLong chimeras = new AtomicLong(0);
            private AtomicLong chimerasDenominator = new AtomicLong(0);
            private AtomicLong adapterReads = new AtomicLong(0);
            private AtomicLong indels = new AtomicLong(0);

            private AtomicLong nonBisulfiteAlignedBases = new AtomicLong(0);
            private AtomicLong hqNonBisulfiteAlignedBases = new AtomicLong(0);
            private final Histogram<Long> mismatchHistogram = new Histogram<Long>();
            private final Histogram<Long> hqMismatchHistogram = new Histogram<Long>();
            private final Histogram<Integer> badCycleHistogram = new Histogram<Integer>();

            private Lock readLengthHistLock = new ReentrantLock();
            private Lock hqMismatchHistLock = new ReentrantLock();
            private Lock mismatchHistLock = new ReentrantLock();
            private Lock badCycleHistLock = new ReentrantLock();

            public IndividualAlignmentSummaryMetricsCollector(final AlignmentSummaryMetrics.Category pairingCategory,
                                                              final String sample,
                                                              final String library,
                                                              final String readGroup) {
                metrics = new AlignmentSummaryMetrics();
                metrics.CATEGORY = pairingCategory;
                metrics.SAMPLE = sample;
                metrics.LIBRARY = library;
                metrics.READ_GROUP = readGroup;
            }

            public void addRecord(final SAMRecord record, final ReferenceSequence ref) {
                if (record.getNotPrimaryAlignmentFlag()) {
                    // only want 1 count per read so skip non primary alignments
                    return;
                }

                collectReadData(record);
                collectQualityData(record, ref);
            }

            public void onComplete() {
                //summarize read data
                if (metrics.TOTAL_READS.get() > 0)
                {
                    metrics.PCT_PF_READS = (double) metrics.PF_READS.get() / (double) metrics.TOTAL_READS.get();
                    metrics.PCT_ADAPTER = this.adapterReads.get() / (double) metrics.PF_READS.get();
                    metrics.MEAN_READ_LENGTH = readLengthHistogram.getMean();

                    //Calculate BAD_CYCLES
                    metrics.BAD_CYCLES = 0;
                    for (final Histogram.Bin<Integer> cycleBin : badCycleHistogram.values()) {
                        final double badCyclePercentage = cycleBin.getValue() / metrics.TOTAL_READS.get();
                        if (badCyclePercentage >= .8) {
                            metrics.BAD_CYCLES++;
                        }
                    }

                    if(doRefMetrics) {
                        if (metrics.PF_READS.get() > 0)         metrics.PCT_PF_READS_ALIGNED = (double) metrics.PF_READS_ALIGNED.get() / (double) metrics.PF_READS.get();
                        if (metrics.PF_READS_ALIGNED.get() > 0) metrics.PCT_READS_ALIGNED_IN_PAIRS = (double) metrics.READS_ALIGNED_IN_PAIRS.get()/ (double) metrics.PF_READS_ALIGNED.get();
                        if (metrics.PF_READS_ALIGNED.get() > 0) metrics.STRAND_BALANCE = numPositiveStrand.get() / (double) metrics.PF_READS_ALIGNED.get();
                        if (this.chimerasDenominator.get() > 0) metrics.PCT_CHIMERAS = this.chimeras.get() / (double) this.chimerasDenominator.get();

                        if (nonBisulfiteAlignedBases.get() > 0) metrics.PF_MISMATCH_RATE = mismatchHistogram.getSum() / (double) nonBisulfiteAlignedBases.get();
                        metrics.PF_HQ_MEDIAN_MISMATCHES = hqMismatchHistogram.getMedian();
                        if (hqNonBisulfiteAlignedBases.get() > 0) metrics.PF_HQ_ERROR_RATE = hqMismatchHistogram.getSum() / (double) hqNonBisulfiteAlignedBases.get();
                        if (metrics.PF_ALIGNED_BASES.get() > 0) metrics.PF_INDEL_RATE = this.indels.get() / (double) metrics.PF_ALIGNED_BASES.get();
                    }
                }
            }

            private void collectReadData(final SAMRecord record) {
                // NB: for read count metrics, do not include supplementary records, but for base count metrics, do include supplementary records.
                if (record.getSupplementaryAlignmentFlag()) return;

                metrics.TOTAL_READS.incrementAndGet();

                readLengthHistLock.lock();
                try {
                    readLengthHistogram.increment(record.getReadBases().length);
                }finally {
                    readLengthHistLock.unlock();
                }

                if (!record.getReadFailsVendorQualityCheckFlag()) {
                    metrics.PF_READS.incrementAndGet();
                    if (isNoiseRead(record)) metrics.PF_NOISE_READS.incrementAndGet();

                    if (record.getReadUnmappedFlag()) {
                        // If the read is unmapped see if it's adapter sequence
                        final byte[] readBases = record.getReadBases();
                        if (!(record instanceof BAMRecord)) StringUtil.toUpperCase(readBases);

                        if (adapterUtility.isAdapterSequence(readBases)) {
                            this.adapterReads.incrementAndGet();
                        }
                    }
                    else if(doRefMetrics) {
                        metrics.PF_READS_ALIGNED.incrementAndGet();
                        if (!record.getReadNegativeStrandFlag()) numPositiveStrand.incrementAndGet();
                        if (record.getReadPairedFlag() && !record.getMateUnmappedFlag()) {
                            metrics.READS_ALIGNED_IN_PAIRS.incrementAndGet();

                            // Check that both ends have mapq > minimum
                            final Integer mateMq = record.getIntegerAttribute("MQ");
                            if (mateMq == null || mateMq >= MAPPING_QUALITY_THRESOLD && record.getMappingQuality() >= MAPPING_QUALITY_THRESOLD) {
                                this.chimerasDenominator.getAndIncrement();

                                // With both reads mapped we can see if this pair is chimeric
                                if (ChimeraUtil.isChimeric(record, maxInsertSize, expectedOrientations)) {
                                    this.chimeras.getAndIncrement();
                                }
                            }
                        }
                        else { // fragment reads or read pairs with one end that maps
                            // Consider chimeras that occur *within* the read using the SA tag
                            if (record.getMappingQuality() >= MAPPING_QUALITY_THRESOLD) {
                                this.chimerasDenominator.getAndIncrement();
                                if (record.getAttribute("SA") != null) this.chimeras.getAndIncrement();
                            }
                        }
                    }
                }
            }

            private void collectQualityData(final SAMRecord record, final ReferenceSequence reference) {
                // NB: for read count metrics, do not include supplementary records, but for base count metrics, do include supplementary records.

                // If the read isn't an aligned PF read then look at the read for no-calls
                if (record.getReadUnmappedFlag() || record.getReadFailsVendorQualityCheckFlag() || !doRefMetrics) {
                    final byte[] readBases = record.getReadBases();
                    for (int i = 0; i < readBases.length; i++) {
                        if (SequenceUtil.isNoCall(readBases[i])) {
                            badCycleHistLock.lock();
                            try{
                                badCycleHistogram.increment(CoordMath.getCycle(record.getReadNegativeStrandFlag(), readBases.length, i));
                            }finally {
                                badCycleHistLock.unlock();
                            }
                        }
                    }
                }
                else if (!record.getReadFailsVendorQualityCheckFlag()) {
                    final boolean highQualityMapping = isHighQualityMapping(record);
                    if (highQualityMapping && !record.getSupplementaryAlignmentFlag()) metrics.PF_HQ_ALIGNED_READS.incrementAndGet();

                    final byte[] readBases = record.getReadBases();
                    final byte[] refBases = reference.getBases();
                    final byte[] qualities  = record.getBaseQualities();
                    final int refLength = refBases.length;
                    long mismatchCount   = 0;
                    long hqMismatchCount = 0;

                    for (final AlignmentBlock alignmentBlock : record.getAlignmentBlocks()) {
                        final int readIndex = alignmentBlock.getReadStart() - 1;
                        final int refIndex  = alignmentBlock.getReferenceStart() - 1;
                        final int length    = alignmentBlock.getLength();

                        for (int i=0; i<length && refIndex+i<refLength; ++i) {
                            final int readBaseIndex = readIndex + i;
                            boolean mismatch = !SequenceUtil.basesEqual(readBases[readBaseIndex], refBases[refIndex+i]);
                            boolean bisulfiteBase = false;
                            if (mismatch && isBisulfiteSequenced &&
                                    record.getReadNegativeStrandFlag() &&
                                    (refBases[refIndex + i] == 'G' || refBases[refIndex + i] == 'g') &&
                                    (readBases[readBaseIndex] == 'A' || readBases[readBaseIndex] == 'a')
                                    || ((!record.getReadNegativeStrandFlag()) &&
                                    (refBases[refIndex + i] == 'C' || refBases[refIndex + i] == 'c') &&
                                    (readBases[readBaseIndex] == 'T') || readBases[readBaseIndex] == 't')) {

                                bisulfiteBase = true;
                                mismatch = false;
                            }

                            if(mismatch) mismatchCount++;

                            metrics.PF_ALIGNED_BASES.incrementAndGet();
                            if(!bisulfiteBase) nonBisulfiteAlignedBases.incrementAndGet();

                            if (highQualityMapping) {
                                metrics.PF_HQ_ALIGNED_BASES.incrementAndGet();
                                if (!bisulfiteBase) hqNonBisulfiteAlignedBases.incrementAndGet();
                                if (qualities[readBaseIndex] >= BASE_QUALITY_THRESHOLD) metrics.PF_HQ_ALIGNED_Q20_BASES.incrementAndGet();
                                if (mismatch) hqMismatchCount++;
                            }

                            if (mismatch || SequenceUtil.isNoCall(readBases[readBaseIndex])) {
                                badCycleHistLock.lock();
                                try {
                                    badCycleHistogram.increment(CoordMath.getCycle(record.getReadNegativeStrandFlag(), readBases.length, i));
                                } finally {
                                    badCycleHistLock.unlock();
                                }
                            }
                        }
                    }

                    mismatchHistLock.lock();
                    try {
                        mismatchHistogram.increment(mismatchCount);
                    } finally {
                        mismatchHistLock.unlock();
                    }
                    hqMismatchHistLock.lock();
                    try {
                        hqMismatchHistogram.increment(hqMismatchCount);
                    } finally {
                        hqMismatchHistLock.unlock();
                    }

                    //hqMismatchHistogram.incrementAndGet(hqMismatchCount, 1d);

                    // Add any insertions and/or deletions to the global count
                    for (final CigarElement elem : record.getCigar().getCigarElements()) {
                        final CigarOperator op = elem.getOperator();
                        if (op == CigarOperator.INSERTION || op == CigarOperator.DELETION) this.indels.getAndIncrement();
                    }
                }
            }

            private boolean isNoiseRead(final SAMRecord record) {
                final Object noiseAttribute = record.getAttribute(ReservedTagConstants.XN);
                return (noiseAttribute != null && noiseAttribute.equals(1));
            }

            private boolean isHighQualityMapping(final SAMRecord record) {
                return !record.getReadFailsVendorQualityCheckFlag() &&
                        record.getMappingQuality() >= MAPPING_QUALITY_THRESOLD;
            }

            public AlignmentSummaryMetrics getMetrics() {
                return this.metrics;
            }
        }
    }
}
