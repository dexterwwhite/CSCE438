/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package nlrr;

import java.util.Arrays;
import java.io.BufferedReader;
import org.apache.hadoop.mapreduce.RecordReader;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.io.LongWritable;
import java.io.IOException;
import java.io.InputStreamReader;
import org.apache.hadoop.mapreduce.InputSplit;
import org.apache.hadoop.mapreduce.TaskAttemptContext;
import org.apache.hadoop.mapreduce.lib.input.FileSplit;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FileSystem;

/**
 *
 * @author kamal
 */
class NRecordReader extends RecordReader<LongWritable, Text> {

    private LongWritable key;
    private Text value;
    private FileSplit fsplit;
    private Configuration conf;
    private FSDataInputStream fsinstream;
    private FileSystem fs;
    private long splitstart = 0;
    private long splitlen = 0;
    private long bytesread = 0;
    private BufferedReader br;
    private final int newlinebytes = ("\n").getBytes().length;
    private int NLinesToRead = 2;

    @Override
    public void initialize(InputSplit split, TaskAttemptContext context)
            throws IOException {

/*  Initialize Input stream and reader. We use stream to seek to start of
    split position and reader to readlines
*/

        this.fsplit = (FileSplit) split;
        this.conf = context.getConfiguration();

        fs = fsplit.getPath().getFileSystem(conf);
        fsinstream = fs.open(fsplit.getPath());
        br = new BufferedReader(new InputStreamReader(fsinstream));

//  RecordLength field is set in driver program and informs 
//  how many lines make a record

        NLinesToRead = conf.getInt("RecordLength",0);

        splitstart = fsplit.getStart();
        splitlen = fsplit.getLength();

/*  LineInputFormat, which we use as our input format, set the length of first
    split to 1 less byte and for other split it decrements one byte from 
    splitstart.  This it does to ensure it works hand in had with LineRecordReader.
    Since we are writing our own record reader, we need to offset this so that
    split boundaries line-up with line boundaries.
*/       
        if (splitstart == 0) splitlen++;
        else splitstart++;
        
        fsinstream.skip(splitstart);
    }

// Read N lines as one record.  
    
    public String readNLines() throws IOException {
        String Nlines = null;
        String line = null;
        for (int i = 0; (i < NLinesToRead) && ((line = br.readLine()) != null); i++) {
            if (Nlines == null) {
                Nlines = line;
            } else {
                Nlines = Nlines.concat(line);
            }
        }
        return Nlines;
    }

/* Read Nline records till split boundary. NewLine character length is added to 
   overall count of bytes read as it is lost during readline.
*/
    @Override
    public boolean nextKeyValue() throws IOException {
        if (bytesread >= splitlen ) {
            return false;
        } else {
            String line;
            if ((line = readNLines()) != null) {
				int bytes = line.getBytes().length + newlinebytes * NLinesToRead;
				if(bytes + bytesread >= splitlen)
				{
					line = " ";
				}
				value = new Text(line);
				key = new LongWritable(splitstart);
                bytesread += bytes;
                return true;
            } else {
                return false;
            }
        }
    }

    @Override
    public void close() throws IOException {
// do nothing
    }

    @Override
    public LongWritable getCurrentKey() {
        return this.key;
    }

    @Override
    public Text getCurrentValue() {
        return this.value;
    }

    @Override
    public float getProgress() throws IOException {
        return true ? 1.0f : 0.0f;
    }
}