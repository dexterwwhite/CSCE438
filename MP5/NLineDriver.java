/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
 package nlrr;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.conf.Configured;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.lib.input.NLineInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;
import org.apache.hadoop.util.Tool;
import org.apache.hadoop.util.ToolRunner;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import java.io.IOException;
import java.util.StringTokenizer;

/**
 *
 * @author kamal
 */
public class NLineDriver extends Configured implements Tool {

	public static class TokenizerMapper extends Mapper<Object, Text, Text, LongWritable> {

		private final static LongWritable one = new LongWritable(1);
		private Text word = new Text();

		public void map(Object key, Text value, Context context) throws IOException, InterruptedException {
			// StringTokenizer itr = new StringTokenizer(value.toString());
			// while (itr.hasMoreTokens()) {
				// word.set(itr.nextToken());
				// context.write(word, one);
			// }
		  // MY OLD CODE
		  // String line = value.toString();
		  // if(line.length() > 0 && line.charAt(0) == 'T')
		  // {
			  // int index = line.indexOf(":");
			  // word.set(line.substring(index - 2, index));
			  // context.write(word, one);
		  // }
		  
		  String line = value.toString();
		  if(line.length() > 0)
		  {
			  int index = line.indexOf("total number:");
			  if(index < 0)
			  {
				  index = 0;
				  //line = line.substring(index + 13, line.length());
			  }
			  else
			  {
				  index += 13;
			  }
			  index = line.indexOf(":", index);
			  if(index >= 2)
			  {
				  word.set(line.substring(index - 2, index));
				  index = line.indexOf("W");
				  if(index >= 0)
				  {
					  //line = line.substring(index, line.length());
					  if(line.indexOf("sleep", index) >= 0)
						  context.write(word, one);
				  }
			  }
		  }
		}
	}

	public static class LongSumReducer
	   extends Reducer<Text,LongWritable,Text,LongWritable> {
	private LongWritable result = new LongWritable();

	public void reduce(Text key, Iterable<LongWritable> values,
					   Context context
					   ) throws IOException, InterruptedException {
	  long sum = 0;
	  for (LongWritable val : values) {
		sum += val.get();
	  }
	  result.set(sum);
	  context.write(key, result);
	}
	}

    @Override
    public int run(String[] args) throws Exception {
        if (args.length != 2) {
            System.err.printf("Usage: %s [generic options] <input> <output>\n",
                    getClass().getSimpleName());
            ToolRunner.printGenericCommandUsage(System.err);
            return -1;
        }

	//  Set RecordLenght configuration parameter so that is it accessible to 
	//  individual mappers
        Configuration conf = getConf();
        conf.setInt("RecordLength", 4);
        Job job = Job.getInstance(conf, "Test Input Format");

	//  We do not need reducers for this demonstration        
        //job.setNumReduceTasks(0);

	//  Set InputFormat to our customised input format.
        job.setInputFormatClass(nlrr.IFNLine.class);
        
	//  We need all splits, except last, to be multiple of recordlenght.  This is 
	//  only way to ensure we are not troubled by split boundaries.

        NLineInputFormat.setNumLinesPerSplit(job, 4 * 300000);
        job.setJarByClass(getClass());
        
		job.setMapperClass(TokenizerMapper.class);
		job.setCombinerClass(LongSumReducer.class);
		job.setReducerClass(LongSumReducer.class);
		
        
	//  Set your input and output path and delete output directory.
        NLineInputFormat.addInputPath(job, new Path(args[0]));
        
        FileOutputFormat.setOutputPath(job, new Path(args[1]));
        
        FileSystem filesystem = FileSystem.get(getConf());
        filesystem.delete(new Path(args[1]), true);
		
		job.setOutputKeyClass(Text.class);
		job.setOutputValueClass(LongWritable.class);
        //job.setOutputValueClass(Text.class);
        //job.setOutputKeyClass(LongWritable.class);
        return job.waitForCompletion(true) ? 0 : 1;
		
		
		// //Configuration conf = new Configuration();
		// Job job = Job.getInstance(conf, "word count");
		// //job.setJarByClass(WordCount.class);
		// job.setMapperClass(TokenizerMapper.class);
		// job.setCombinerClass(IntSumReducer.class);
		// job.setReducerClass(IntSumReducer.class);
		// job.setOutputKeyClass(Text.class);
		// job.setOutputValueClass(IntWritable.class);
		// FileInputFormat.addInputPath(job, new Path(args[0]));
		// FileOutputFormat.setOutputPath(job, new Path(args[1]));
		// System.exit(job.waitForCompletion(true) ? 0 : 1);
    }

    public static void main(String[] args) throws Exception {
        int exitCode = ToolRunner.run(new NLineDriver(), args);
        System.exit(exitCode);
    }
} 