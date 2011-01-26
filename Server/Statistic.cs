using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace FRESCO_Server
{
    enum OutFlags
    {
        outYear = 0x1,									//Output the data averaged by year
        outRep = 0x2,									//Output the data averaged by replicate
        outFormat = 0x4,									//Format in a way to facilitate reading into a stats package for analysis
        outNum = 0x10,									//Output the number of samples
        outMean = 0x20,									//Output the mean of the data
        outStdDev = 0x40,									//Output the standard deviation of the samples
        outMin = 0x80,									//Output the minimum of the samples
        outMax = 0x100,								//Output the maximum of the samples
        outHist = 0x200,								//Output the histogram data if there is any
        outData = 0x400,								//Output the file of data.
        outEvents = 0x800									//Output the file of events.
    };


    class Statistic
    {
        //Variables
        public long tally;                                  //For totaling incremental types.
        private string title;                               //Title of the stat contents.
        private int maxReps;								//Maintain a local copy for optimization.
        private int outFlags;								//Indicates which data to output when the object is written.
        private List<List<double>> data;
        private List<BasicStatistic> basicStatYear;	        //Maintains the statistics across replicates
        private List<BasicStatistic> basicStatRep;		    //Maintains the statistics within a replicate
        private LinkedList<List<double>>[,] eventLists;     //A bucket system is used to avoid finding contiguous memory for all events.  Instead a list of buckets containing data events is maintained.
        private const int BUCKET_CAPACITY = 10000;


        /// <summary>
        /// Constructor.  Initializes members and allocates memory.
        /// </summary>
        /// <param name="title"></param>
        /// <param name="maxYears"></param>
        /// <param name="maxReps"></param>
        /// <param name="outflags"></param>
        public Statistic(string title, int outflags)
        {
            //Setup the default values.
            this.tally = 0;
            this.title = title;
            this.maxReps = Global.Instance.FIF.MaxReps;
            this.outFlags = outflags;

            int numYears = Global.Instance.FIF.LastYear - Global.Instance.FIF.FirstYear + 1;

            //Allocate memory.
            basicStatYear = new List<BasicStatistic>(numYears);  //Init capacity to include the last year in the sequence
            for (int i = 0; i < basicStatYear.Capacity; i++)
                basicStatYear.Add(new BasicStatistic());

            basicStatRep = new List<BasicStatistic>(maxReps);
            for (int i = 0; i < basicStatRep.Capacity; i++)
                basicStatRep.Add(new BasicStatistic());

            data = new List<List<double>>(numYears);
            for (int y = 0; y < data.Capacity; y++)
            {
                data.Add(new List<double>(maxReps));
                for (int r = 0; r < data[y].Capacity; r++)
                    data[y].Add(0.0);
            }

            eventLists = new LinkedList<List<double>>[numYears, maxReps];
            for (int y = 0; y < numYears; y++)
            {
                for (int r = 0; r < maxReps; r++)
                {
                    eventLists[y, r] = new LinkedList<List<double>>();
                    eventLists[y, r].AddLast(new List<double>());
                }
            }
        }

        //Functions
        /// <summary>
        /// Resets the stat to values at creation.
        /// </summary>
        public void Clear()
        {
            tally = 0;
            //Reset year and rep stats.
            basicStatYear.Clear();
            basicStatRep.Clear();
            //Reset data array.
            for (int i = 0; i < data.Count; i++)
                data[i].Clear();
            data.Clear();
            ClearEvents();
        }

        /// <summary>
        /// Adds a given statistic to the year and replicate.  If an invalid Year/Rep is specified, an error is thrown.
        /// </summary>
        /// <param name="year"></param>
        /// <param name="rep"></param>
        /// <param name="data"></param>
        /// <param name="cause"></param>
        public void Add(int year, int rep, double value)
        {
            //Error if year or rep is out of bounds.
            if (year > Global.Instance.FIF.LastYear || year < Global.Instance.FIF.FirstYear) throw new Exception("Invalid year specified when adding a statisitc.\n");
            if (rep >= maxReps || rep < 0) throw new Exception("Invalid replicate specified when adding a statisitc.\n");
            //Add data to the by year statistic.
            basicStatYear[year - Global.Instance.FIF.FirstYear].Add(value);
            //Add data to the by rep statistic.
            basicStatRep[rep].Add(value);
            //Add data to the summation for year and rep.
            data[year - Global.Instance.FIF.FirstYear][rep] += value;
            //Add data to the event list. 
            if ((outFlags & (int)OutFlags.outEvents) > 0)
                AddEvent(year, rep, value);
        }

        /// <summary>
        /// Increment the tally by one.
        /// </summary>
        /// <param name="a"></param>
        /// <returns></returns>
        public static Statistic operator ++(Statistic a)
        {
            a.tally++;
            return a;
        }

        /// <summary>
        /// Decrement the tally by one.
        /// </summary>
        /// <param name="a"></param>
        /// <returns></returns>
        public static Statistic operator --(Statistic a)
        {
            a.tally--;
            return a;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="year"></param>
        /// <param name="rep"></param>
        /// <returns></returns>
        public double SumOfYearRep(int year, int rep)
        {
            return data[year - Global.Instance.FIF.FirstYear][rep];
        }

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public double SumAcrossYears()
        {
            double value = 0;
            for (int i = 0; i < basicStatYear.Count; i++)
                value += basicStatYear[i].Sum;
            return value;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public double SumAcrossReps()
        {
            double value = 0;
            for (int i = 0; i < basicStatRep.Count; i++)
                value += basicStatRep[i].Sum;
            return value;
        }

        /// <summary>
        /// Writes this object's stats to the stream according to the outFlags.
        /// </summary>
        /// <param name="stream">The stream to be written to.</param>
        public void Save()
        {
            ////Output title as header.
            //sStream << title.c_str() << endl;
            ////Output within year data
            //int bFormat = outFlags & OutFlags.outFormat;
            //if (outFlags & OutFlags.outYear) {
            //    if (bFormat) {
            //        sStream << "Year:";
            //        for (int Year = 0; Year <= maxYears; Year++)
            //            sStream << "\t" << Year * timeStep;
            //        sStream << endl;
            //    }
            //    if (outFlags & outNum) {
            //        if (bFormat) sStream << "Num:";
            //        for (int Year = 0; Year <= maxYears/timeStep; Year++)
            //            sStream << "\t" << basicStatYear[Year].Num();
            //        sStream << endl;
            //    }
            //    if (outFlags & outMean) {
            //        if (bFormat) sStream << "Mean:";
            //        for (int Year = 0; Year <= maxYears/timeStep; Year++)
            //            sStream << "\t" << basicStatYear[Year].Mean();
            //        sStream << endl;
            //    }
            //    if (outFlags & outStdDev) {
            //        if (bFormat) sStream << "Std:";
            //        for (int Year = 0; Year <= maxYears/timeStep; Year++)
            //            sStream << "\t" << basicStatYear[Year].StdDev();
            //        sStream << endl;
            //    }
            //    if (outFlags & outMin) {
            //        if (bFormat) sStream << "Min:";
            //        for (int Year = 0; Year <= maxYears/timeStep; Year++)
            //            sStream << "\t" << basicStatYear[Year].Min();
            //        sStream << endl;
            //    }
            //    if (outFlags & outMax) {
            //        if (bFormat) sStream << "Max:";
            //        for (int Year = 0; Year <= maxYears/timeStep; Year++)
            //            sStream << "\t" << basicStatYear[Year].Max();
            //        sStream << endl;
            //    }
            //}

            ////Output across year data
            //if (outFlags & outRep) {
            //    if (!bFormat) {
            //        sStream << "Rep:";
            //        for (int Rep = 0; Rep < maxReps; Rep++)
            //            sStream << "\t" << Rep;
            //        sStream << endl;
            //    }
            //    if (outFlags & outNum) {
            //        if (bFormat) sStream << "Num:";
            //        for (int Rep = 0; Rep < maxReps; Rep++)
            //            sStream << "\t" << basicStatRep[Rep].Num();
            //        sStream << endl;
            //    }
            //    if (outFlags & outMean) {
            //        if (bFormat) sStream << "Mean:";
            //        for (int Rep = 0; Rep < maxReps; Rep++)
            //            sStream << "\t" << basicStatRep[Rep].Mean();
            //        sStream << endl;
            //    }
            //    if (outFlags & outStdDev) {
            //        if (bFormat) sStream << "Std:";
            //        for (int Rep = 0; Rep < maxReps; Rep++)
            //            sStream << "\t" << basicStatRep[Rep].StdDev();
            //        sStream << endl;
            //    }
            //    if (outFlags & outMin) {
            //        if (bFormat) sStream << "Min:";
            //        for (int Rep = 0; Rep < maxReps; Rep++)
            //            sStream << "\t" << basicStatRep[Rep].Min();
            //        sStream << endl;
            //    }
            //    if (outFlags & outMax) {
            //        if (bFormat) sStream << "Max:";
            //        for (int Rep = 0; Rep < maxReps; Rep++)
            //            sStream << "\t" << basicStatRep[Rep].Max();
            //        sStream << endl;
            //    }
            //}

            //Output data.
            if ((outFlags & (int)OutFlags.outData) > 0)
            {
                System.IO.StreamWriter file = new System.IO.StreamWriter(Path.Combine(Global.Instance.StatOutputDirectory, title + ".txt"), false);
                file.Write("Year");
                for (int r = 0; r < maxReps; r++) file.Write("\tRep " + r);
                file.WriteLine("");
                for (int y = Global.Instance.FIF.FirstYear; y <= Global.Instance.FIF.LastYear; y++)
                {
                    file.Write(y + "\t");
                    for (int r = 0; r < maxReps; r++)
                    {
                        file.Write(data[y - Global.Instance.FIF.FirstYear][r] + "\t");
                    }
                    file.WriteLine("");
                    file.Flush();
                }
                file.WriteLine("");
                file.Close();
            }

            //Output events.
            if ((outFlags & (int)OutFlags.outEvents) > 0)
            {
                System.IO.FileStream file = new System.IO.FileStream(Path.Combine(Global.Instance.StatOutputDirectory, title + "Events.txt"), System.IO.FileMode.Create);
                System.IO.StreamWriter stream = new System.IO.StreamWriter(file);
                stream.WriteLine("Year\tRep\tValue");
                string rep;
                string yearAndRep;
                for (int r = 0; r < maxReps; r++)
                {
                    rep = r + "\t";
                    for (int y = Global.Instance.FIF.FirstYear; y <= Global.Instance.FIF.LastYear; y++)
                    {
                        yearAndRep = y + "\t" + rep;
                        foreach (List<double> eventList in eventLists[y - Global.Instance.FIF.FirstYear, r])
                        {
                            foreach (double value in eventList)
                            {
                                stream.WriteLine(yearAndRep + value);
                            }
                        }
                    }
                }
                stream.Flush();
                stream.Close();
                file.Close();
            }
        }

        private void ClearEvents()
        {
            for (int y = Global.Instance.FIF.FirstYear; y <= Global.Instance.FIF.LastYear - Global.Instance.FIF.FirstYear; y++)
            {
                for (int r = 0; r < maxReps; r++)
                {
                    foreach (List<double> eventList in eventLists[y, r])
                    {
                        eventList.Clear();
                        eventList.Capacity = 0;
                    }
                    eventLists[y - Global.Instance.FIF.FirstYear, r].Clear();
                    eventLists[y - Global.Instance.FIF.FirstYear, r].AddLast(new List<double>());  //To return to state after Statistic constructor.
                }
            }
        }

        private void AddEvent(int year, int rep, double value)
        {
            try
            {
                List<double> curentBucket = eventLists[year - Global.Instance.FIF.FirstYear, rep].Last.Value;
                try { curentBucket.Add(value); }
                catch (System.OutOfMemoryException)
                {
                    //Ideally all new buckets are created when checking capacity below.
                    curentBucket = eventLists[year - Global.Instance.FIF.FirstYear, rep].AddLast(new List<double>(BUCKET_CAPACITY)).Value;
                    curentBucket.Add(value);
                }
                // Check capacity and start new bucket if needed (First bucket might not be == 'capacity').
                // ?lock?
                if (curentBucket.Count == curentBucket.Capacity && curentBucket.Count >= BUCKET_CAPACITY)
                {
                    curentBucket = eventLists[year - Global.Instance.FIF.FirstYear, rep].AddLast(new List<double>(BUCKET_CAPACITY)).Value;
                }
            }
            catch (System.OutOfMemoryException e)
            {
                string m = "";
                m += "Failed adding statistical event to " + title + " stat due to System.OutOfMemoryException.\n";
                m += "Event year=" + year + " and rep=" + rep + ".  \n";
                m += "Number of buckets for this year and rep: " + eventLists[year - Global.Instance.FIF.FirstYear, rep].Count + "\n";
                m += "Current bucket's count=" + eventLists[year - Global.Instance.FIF.FirstYear, rep].Last.Value.Count;
                m += " and capacity=" + eventLists[year - Global.Instance.FIF.FirstYear, rep].Last.Value.Capacity + "\n";
                throw new Exception(m, e);
            }
        }
    }
}