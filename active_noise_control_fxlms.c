//+++++++++++++++++++++++++++++++++++++++++++ commentaries +++++++++++++++++++++++++++++++++++++++++++++++++++++
/*{{{
 *This is a  int-out implementation use a map areas, i use this file for put the same gain in two microphones 
 *
 *
 *}}}*/
//+++++++++++++++++++++++++++++++++++++++++++ Libraries +++++++++++++++++++++++++++++++++++++++++++++++++++++
/*{{{*/
	
	#define _GNU_SOURCE
	#include <alsa/asoundlib.h>
	#include <math.h>
	#include <signal.h>
	#include "genera_ruta_secundaria.h"
	#include "filtro_adatado.h"
	#include "genera_noise_fijo_3.h"
	
/*}}}*/
//++++++++++++++++++++++++++++++++++++++ parameters for use +++++++++++++++++++++++++++++++++++++++++++++++++
/*{{{*/
	
	static char *device_playback = "plughw:1,0" ;                                        /* playback device */
	static char *device_capture  = "plughw:1,0" ;                                        /* playback device */
	static snd_pcm_stream_t playback_card = SND_PCM_STREAM_PLAYBACK; 
	static snd_pcm_stream_t record_card   = SND_PCM_STREAM_CAPTURE;          
	static snd_pcm_access_t access_pcm    = SND_PCM_ACCESS_MMAP_INTERLEAVED;   /* type access                          */
	static snd_pcm_format_t format = SND_PCM_FORMAT_S16;                       /* sample format                        */
	static unsigned int rate = 4000;                                           /* stream rate                          */
	static unsigned int channels_playback = 1 ;                                 /* count of channels                    */
	static unsigned int channels_capture  = 2 ;                                 /* count of channels                    */
	static unsigned int buffer_time = 2000 ;                                   /* ring buffer length in us             */
	static unsigned int period_time = 1000 ;                                    /* period time in us                    */
	static int resample = 1;                                                   /* enable alsa-lib resampling           */
	static int period_event = 0;                                               /* produce poll event after each period */
	static snd_pcm_sframes_t buffer_size;
	static snd_pcm_sframes_t period_size;
	static snd_pcm_sframes_t period_cal;
	const snd_pcm_channel_area_t *my_areas_capture;
	snd_pcm_uframes_t offset_capture, frames_capture; 
	const snd_pcm_channel_area_t *my_areas_playback;
	snd_pcm_uframes_t offset_playback, frames_playback;
	FILE  *File_error ;
	int16_t Array_error[ SIZE_X_NOISE ] ;
	snd_pcm_t *handle_playback_device, *handle_record_device;
	int final_close();
	
/*}}}*/
//+++++++++++++++++++++++++++++++ configuration hardware and software parameters ++++++++++++++++++++++++++++
int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params,snd_pcm_access_t access)
{/*{{{*/
	
	unsigned int rrate;
	unsigned int channels;
	snd_pcm_uframes_t size_buffer_size,size_period_size;
	int err, dir;
	
	/* choose all parameters */
	if ((err = snd_pcm_hw_params_any(handle, params)) < 0) 
	{
		
		fprintf(stdout,"Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
		
	}
	
	/* set hardware resampling */
	if ((err = snd_pcm_hw_params_set_rate_resample(handle, params, resample)) < 0)
	{
		
		fprintf(stdout,"Resampling setup failed for playback: %s\n", snd_strerror(err));
		return err;
		
	}
	
	/* set the interleaved read/write format */
	if ((err = snd_pcm_hw_params_set_access(handle, params, access)) < 0)
	{
		
		printf("Access type not available for playback: %s\n", snd_strerror(err));
		return err;
		
	}
	
	/* set the sample format */
	if (( err = snd_pcm_hw_params_set_format(handle, params, format)) < 0)
	{
		
		printf("Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
		
	}
	
	if ( snd_pcm_stream ( handle ) == SND_PCM_STREAM_PLAYBACK  )
	{
		
		channels = channels_playback ;
		
	}
	else if ( snd_pcm_stream ( handle ) == SND_PCM_STREAM_CAPTURE )
	{
		
		channels = channels_capture ;
		
	}
	else
	{
		
		printf("error no find stream in controler\n");
		exit(EXIT_FAILURE);
		
	}
	
	/* set the count of channels */
	if ((err = snd_pcm_hw_params_set_channels(handle, params, channels)) < 0) 
	{
		
		printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
		return err;
		
	}
	
	/* set the stream rate */
	rrate = rate;
	if ((err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0)) < 0) 
	{
		
		printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
		return err;
		
	}
	
	if (rrate != rate)
	{
		
		printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
		return err;
		
	}
	
	/* set the buffer time */
	if ( ( err = snd_pcm_hw_params_set_buffer_time_near( handle, params, &buffer_time, &dir ) ) < 0)
	{
		
		printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
		return err;
		
	}
	
	if ((err = snd_pcm_hw_params_get_buffer_size(params, &size_buffer_size)) < 0)
	{
		
		printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
		return err;
		
	}
	buffer_size = size_buffer_size;
	
	/* set the period time */
	if ((err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir)) < 0) 
	{
		
		printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
		return err;
		
	}
	
	if ((err = snd_pcm_hw_params_get_period_size(params, &size_period_size, &dir)) < 0)
	{
		
		printf("Unable to get period size for playback: %s\n", snd_strerror(err));
		return err;
		
	}
	
	period_size = size_period_size;
	 
	/* write the parameters to device */
	if ((err = snd_pcm_hw_params(handle, params)) < 0)
	{
		
		printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
		
	}
	
	return 0;
	
}/*}}}*/
int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{/*{{{*/
	int err;
	/* get the current swparams */
	if ((err = snd_pcm_sw_params_current(handle, swparams)) < 0)
	{
		
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	period_cal = (buffer_size / period_size) * period_size;
	
	if (( err = snd_pcm_sw_params_set_start_threshold(handle, swparams,period_cal)) < 0) 
	{
		
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	
	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	if ((err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size)) < 0)
	{
		
		printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	
	/* enable period events when requested */
	if (period_event)
	{
		
		if ((err = snd_pcm_sw_params_set_period_event(handle, swparams, 1)) < 0) 
		{
			
			printf("Unable to set period event: %s\n", snd_strerror(err));
			exit(EXIT_FAILURE);
			
		}
		
	}
	
	/* write the parameters to the playback device */
	if ((err = snd_pcm_sw_params(handle, swparams)) < 0)
	{
		
		printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	
	return 0;
}/*}}}*/
snd_pcm_t* set_opem_sw_hw (snd_pcm_t *handle, char *device, snd_pcm_stream_t stream,
/*{{{*/                           snd_pcm_access_t access_pcm,snd_pcm_hw_params_t *hwparams,snd_pcm_sw_params_t *swparams)
{
	int err,dir;
	unsigned int val; 
	snd_output_t *output = NULL;
	
//++++++++++++++++++++++++++++++++open a set parameters++++++++++++++++++++++++++++++++++
	
	if ( ( err = snd_pcm_open( &handle, device, stream, 0 ) ) < 0 )
	{
		
		printf( "Playback open error: %s\n", snd_strerror(err) );
		exit(EXIT_FAILURE);
		
	}
	
	if ( ( err = set_hwparams(handle, hwparams, access_pcm ) ) < 0 )
	{
		
		fprintf(stdout,"Setting of hwparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	
	if ( ( err = set_swparams(handle, swparams ) ) < 0)
	{
		
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	
//+++++++++++++++++++++++++++ for information of code++++++++++++++++++++++++++++++++++++
	
	if ( ( err = snd_output_stdio_attach( &output, stdout, 0 ) ) < 0 )
	{
		
		printf( "Output failed: %s\n", snd_strerror(err) ); 
		exit(EXIT_FAILURE);
		
	} 
	
	if ((err = snd_pcm_dump_setup(handle, output)) < 0)
	{
		
		printf("Output failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	
	if ( ( err = snd_pcm_hw_params_get_buffer_time( hwparams, &val, &dir) ) < 0)
	{
		
		printf("Unable to get buffer time: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	printf("  buffer time  : %d\n",val);
	
	if((err=snd_pcm_hw_params_get_periods (hwparams, &val, &dir))<0)
	{
		
		printf("Unable to get periods: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
		
	}
	printf("  periods      : %d\n",val);
	printf("\n");
	return handle;
	
}/*}}}*/   
//+++++++++++++++++++++++++++++++ configuration capture and playback device +++++++++++++++++++++++++++++++ 
snd_pcm_t* ini_playback_device()
{/*{{{*/
	
	snd_pcm_t *handle_playback = NULL ;
	snd_pcm_hw_params_t *hwparams_play ;
	snd_pcm_sw_params_t *swparams_play ;
	snd_pcm_hw_params_malloc ( &hwparams_play ) ;
	snd_pcm_sw_params_malloc ( &swparams_play ) ;
	handle_playback = set_opem_sw_hw ( handle_playback , device_playback , playback_card , access_pcm , hwparams_play , swparams_play );
	return handle_playback ;
	
}/*}}}*/
snd_pcm_t* ini_record_device()
{/*{{{*/
	
	snd_pcm_t *handle_record = NULL;
	snd_pcm_hw_params_t *hwparams_record ;
	snd_pcm_sw_params_t *swparams_record ;
	snd_pcm_hw_params_malloc ( &hwparams_record ) ;
	snd_pcm_sw_params_malloc ( &swparams_record ) ;
	handle_record = set_opem_sw_hw ( handle_record , device_capture , record_card , access_pcm , hwparams_record , swparams_record ) ;
	return handle_record ;
	
}/*}}}*/
//+++++++++++++++++++++++++++++++++++++++++++ solve problems underrun ++++++++++++++++++++++++++++++++++++++
 int xrun_recovery(snd_pcm_t *handle, int err)
{/*{{{*/
	if (err == -EPIPE) 
	{    /* under-run */
	      
		if ((err = snd_pcm_prepare(handle)) < 0)
		{
			
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
			
		}
		return 0;
	}
	else if (err == -ESTRPIPE)
	{
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
		{
			
			sleep(1);       /* wait until the suspend flag is released */
			
		}
		
		if (err < 0) 
		{
			
			if ((err = snd_pcm_prepare(handle)) < 0)
			{
				
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
				
			}
			
		}
		return 0;
		
	}
	return err;
	
}/*}}}*/
//+++++++++++++++++++++++++++++++++++++++++++++++ thread capture ++++++++++++++++++++++++++++++++++++++++++
int Capture_Playback(snd_pcm_t  *handle_record , snd_pcm_t *handle_playback)
{/*{{{*/
	
	snd_pcm_uframes_t size_capture ;
	snd_pcm_sframes_t avail_capture , commitres_capture ;
	snd_pcm_state_t state_capture ;
	int err_capture , first_capture = 1 , chn_capture = 0 , eq_capture = 0  , flag_show = 1;
	int16_t *samples_capture[channels_capture] ;
	
	snd_pcm_uframes_t  size_playback ;
	snd_pcm_sframes_t avail_playback , commitres_playback ;
	snd_pcm_state_t state_playback ;
	int16_t *samples_playback[channels_playback] ;
	int err_playback , first_playback = 1 , chn_playback = 0 ;
	
	int  j = 0 , j0 = 0 , i = 0 ;
	float XC = 0 , Y = 0 , YS = 0 ;
	double mu = 0.00000000000001 ;
      /*double mu = 0.0000000000001 ;*/
	double mu_model = 0.00000000000001 ;
	double leak = 0.01 ;
	int size_of_filters =  ( ( (  size_X_filter_adaptado > size_ruta_secundaria) ) ?  size_X_filter_adaptado : size_ruta_secundaria ) ;
	int size_of_rutas =  ( ( (  size_ruta_secundaria > size_ruta_secundaria) ) ?  size_ruta_secundaria : size_ruta_secundaria ) ;

	float *buffer_x ;
	float *buffer_y ;
	float *buffer_e ;
	buffer_x = (float *) malloc( (size_t) ( ( period_size + size_of_filters - 1  ) * sizeof( float )  ) ) ;
	buffer_y = (float *) malloc( (size_t) ( ( size_of_rutas + 1 ) * sizeof( float )  ) ) ;
	buffer_e = (float *) malloc( (size_t) ( ( period_size ) * sizeof( float )  ) ) ;
	float buffer_xc[ size_X_filter_adaptado + 1 ] = {0} ;
	buffer_xc[ size_X_filter_adaptado ] = 0 ;
	float error_es ;
	
	while (1) 
	{
		/*see the state of a handle if its state is xrun, trated to cahnge  */ /*{{{*/
		if ( ( state_playback = snd_pcm_state(handle_playback) ) == SND_PCM_STATE_XRUN ) 
		{
			if ( (err_playback = xrun_recovery( handle_playback, -EPIPE ) ) < 0 ) 
			{
				
				printf("XRUN recovery failed: %s\n", snd_strerror(err_playback));
				exit(EXIT_FAILURE);
				
			}
			first_playback = 1;
			
		}
		
		/*if state is suspended trated to change for running */
		else if ( state_playback == SND_PCM_STATE_SUSPENDED ) 
		{
		 
			if ( (err_playback = xrun_recovery( handle_playback, -ESTRPIPE ) ) < 0 ) 
			{
				printf("SUSPEND recovery failed: %s\n", snd_strerror(err_playback));
				exit(EXIT_FAILURE);
			}
			
		}
		
		if ( ( avail_playback = snd_pcm_avail_update(handle_playback) ) < 0 ) 
		{
			
			if ( ( err_playback = xrun_recovery( handle_playback, avail_playback ) ) < 0) 
			{
				printf("avail_playback update failed: %s\n", snd_strerror(err_playback));
				exit(EXIT_FAILURE);
			}
			first_playback = 1;
			continue;
			
		}
		
		if (avail_playback < period_size) 
		{
			
			if (first_playback)
			{       
				first_playback = 0;
				if ((err_playback = snd_pcm_start(handle_playback))< 0) 
				{
					
					printf("Start error: %s\n", snd_strerror(err_playback));
					exit(EXIT_FAILURE);
					
				}
				
			}
			
			else 
			{
				
				if ( ( err_playback = snd_pcm_wait( handle_playback, -1 ) ) < 0 ) 
				{
					
					if ( (err_playback = xrun_recovery( handle_playback, err_playback ) ) < 0 )
					{
						printf("snd_pcm_wait error: %s\n", snd_strerror(err_playback));
						exit(EXIT_FAILURE);
					}
					first_playback = 1;
					
				}
				
			}
			continue;
		}/*}}}*/
		size_playback = period_size;/*{{{*/
		while (size_playback > 0) 
		{
			frames_playback = size_playback;
			if ((err_playback = snd_pcm_mmap_begin(handle_playback, &my_areas_playback, &offset_playback, &frames_playback)) < 0) 
			{
				
				if ( ( err_playback = xrun_recovery( handle_playback, err_playback ) ) < 0) 
				{
					
					printf("MMAP begin avail error: %s\n", snd_strerror(err_playback));
					exit(EXIT_FAILURE);
					
				}
				first_playback = 1;
				
				
			}
			
			
			commitres_playback = snd_pcm_mmap_commit( handle_playback, offset_playback, frames_playback );
			if ( commitres_playback < 0 || (snd_pcm_uframes_t)commitres_playback != frames_playback ) 
			{
				
				if ( (err_playback = xrun_recovery( handle_playback, commitres_playback >= 0 ? -EPIPE : commitres_playback ) ) < 0 )
				{
					
					printf( "MMAP commit error: %s\n" , snd_strerror(err_playback ) ) ;
					exit(EXIT_FAILURE);
					
				}
				first_playback = 1;
				
			}
			size_playback -= frames_playback;
			
		}/*}}}*/
		/*see the state of a handle if its state is xrun, trated to cahnge  */ /*{{{*/
		if ( ( state_capture = snd_pcm_state(handle_record) ) == SND_PCM_STATE_XRUN ) 
		{
			
			if ( (err_capture = xrun_recovery( handle_record, -EPIPE ) ) < 0 ) 
			{
				
				printf("XRUN recovery failed: %s\n", snd_strerror(err_capture));
				exit(EXIT_FAILURE);
				
			}
			first_capture = 1;
			
		}
		
		/*if state is suspended trated to change for running */
		else if ( state_capture == SND_PCM_STATE_SUSPENDED ) 
		{
		 
			if ( (err_capture = xrun_recovery( handle_record, -ESTRPIPE ) ) < 0 ) 
			{
				
				printf("SUSPEND recovery failed: %s\n", snd_strerror(err_capture));
				exit(EXIT_FAILURE);
				
			}
			
		}
		
		if ( ( avail_capture = snd_pcm_avail_update(handle_record) ) < 0 ) 
		{
			
			if ( ( err_capture = xrun_recovery( handle_record, avail_capture ) ) < 0) 
			{
				
				printf("avail update failed: %s\n", snd_strerror(err_capture));
				exit(EXIT_FAILURE);
				
			}
			first_capture = 1;
			continue;
			
		}
		
		if (avail_capture < period_size) 
		{
			
			if (first_capture)
			{
			 
				first_capture = 0;
				if ((err_capture = snd_pcm_start(handle_record))< 0) 
				{
					
					printf("Start error: %s\n", snd_strerror(err_capture));
					exit(EXIT_FAILURE);
					
				}
				
			}
			
			else 
			{
				
				if ( ( err_capture = snd_pcm_wait( handle_record, -1 ) ) < 0 ) 
				{
					
					if ( (err_capture = xrun_recovery( handle_record, err_capture ) ) < 0 )
					{
						
						printf("snd_pcm_wait error: %s\n", snd_strerror(err_capture));
						exit(EXIT_FAILURE);
						
					}
					first_capture = 1;
					
				}
				
			}
			continue;
		}/*}}}*/
		size_capture = period_size;/*{{{*/
		while (size_capture > 0) 
		{
			
			frames_capture = size_capture;
			if ((err_capture = snd_pcm_mmap_begin(handle_record, &my_areas_capture, &offset_capture, &frames_capture)) < 0) 
			{
				
				if ( ( err_capture = xrun_recovery( handle_record, err_capture ) ) < 0) 
				{
					
					printf("MMAP begin avail error: %s\n", snd_strerror(err_capture));
					exit(EXIT_FAILURE);
					
				}
				first_capture = 1;
				
			}
			
			
			commitres_capture = snd_pcm_mmap_commit( handle_record, offset_capture, frames_capture ); 
			if ( commitres_capture < 0 || (snd_pcm_uframes_t)commitres_capture != frames_capture ) 
			{
				
				if ( (err_capture = xrun_recovery( handle_record, commitres_capture >= 0 ? -EPIPE : commitres_capture ) ) < 0 )
				{
					
					printf( "MMAP commit error: %s\n", snd_strerror(err_capture) );
					exit(EXIT_FAILURE);
					
				}
				first_capture = 1;
				
			}
			size_capture -= frames_capture;
			
		}/*}}}*/
		for (chn_playback = 0; chn_playback < channels_playback; chn_playback++) /*{{{*/
		{
			
			samples_playback[chn_playback] = ( ( (int16_t *) my_areas_playback[chn_playback].addr ) + chn_playback ) + ( offset_playback * channels_playback ) ;
			
		}

		for (chn_capture = 0; chn_capture < channels_capture; chn_capture++) 
		{
			
			samples_capture[chn_capture]  = ( ( (int16_t *) my_areas_capture[chn_capture].addr ) + chn_capture )  + (offset_capture * channels_capture);
			
		}
		
		for( j = size_of_filters - 1 , i = 0 ; i < period_size ; i++ , eq_capture++ , j++ )
		{
			if( eq_capture == SIZE_X_NOISE)
			{
				
				final_close();
				return 0;
				
			}
			
			buffer_x[ j ] = samples_capture[ 0 ][ i * channels_capture ]  ;
			Array_error[ eq_capture ] = buffer_e[ i ] = samples_capture[ 1 ][ i * channels_capture ]  ;
			
		}
		
		for( i = 0 , j0 = 0 ; i < period_size ; i++ , j0 = j0 + 2)
		{

			YS = 0 ;
			for( j = size_ruta_secundaria - 1 ; j >= 0 ; j-- )
			{
				
				YS = YS + ( X_SECU[ j ] * ( buffer_y[ j ] ) ) ;
				
			}
			
			XC = 0 ;
			for( j = 0 ; j < size_ruta_secundaria ; j++ )
			{
				
				XC = XC + ( X_SECU[ j ] * ( buffer_x[ i - j + size_of_filters - 1 ] ) ) ;
				
			}
			
			Y = 0 ;
			for( j = 0 ; j < size_X_filter_adaptado ; j++ )
			{
				
				Y = Y + ( W1_filter[ j ] * ( buffer_x[ i - j + size_of_filters - 1 ] ) ) ;
				buffer_xc[ j ] = buffer_xc[ j + 1 ] ;
				
			}
			
			buffer_xc[ size_X_filter_adaptado - 1 ] =  XC ;
			
			if(eq_capture > 40000 )
			{
				
				if(flag_show)
				{
					
					printf("start to cancel\n") ;
					flag_show = 0 ;
					
				}
				
				for( j = 0 ; j < size_X_filter_adaptado ; j++ )
				{
					
					W1_filter[ j ] =(( 1 - ( mu * leak )) * W1_filter[ j ] ) - (double)( ( buffer_e[ i ] ) * mu * buffer_xc[ size_X_filter_adaptado - 1 - j ] ) ;
					
				} 
				
				samples_playback[ 0 ][ i * channels_playback ] = (int16_t) Y ;
				
			}
			
			error_es = buffer_e[ i ] - YS;
			for( j = size_ruta_secundaria - 1 ; j >= 0 ; j-- )
			{
				
				X_SECU[ j ] =( ( 1 - ( mu * leak ) ) * X_SECU[ j ] ) + (double)( error_es * mu_model  * buffer_y[ j ] ) ;
				buffer_y[j] = buffer_y[ j - 1 ];
				
				
			}
			buffer_y[ 0 ] = (int16_t) Y ;
			
		}
		
		for( i = 0 ; i < size_of_filters ; i++ )
		{
			
			buffer_x[ i ] = buffer_x[ i +period_size ] ;
			
		}/*}}}*/
	}
	return 0;
	
}/*}}}*/
//+++++++++++++++++++++++++++++++++++++++++++++++write final +++++++++++++++++++++++++++++++++++++++++++++++++
int final_close()
{/*{{{*/
	
	printf("final de la ejecucion\n");
	if ( snd_pcm_close(handle_playback_device) < 0 )
	{
		
		printf("error close handle playback\n");
		exit(0);
		
	}
	if ( snd_pcm_close(handle_record_device) < 0)
	{
		
		printf("error close handle record\n");
		exit(0);
		
	}
	
	if( ( File_error = fopen( "Array_error.txt","w" ) ) == NULL )
	{
		
		printf("error to create file time write capture\n");
		exit(EXIT_FAILURE);
		
	}
	
	
	for( int i = 0 ; i < SIZE_X_NOISE ; i++ )
	{
		
		fprintf( File_error , "%d\n" , Array_error[i] ) ;
		
	}
	
	if ( File_error != NULL ) 
	{
		
		fclose( File_error );
		
	}
	printf("end final_close\n");
	return 0;
	
}/*}}}*/
//+++++++++++++++++++++++++++++++++++++++++++++++signal control ++++++++++++++++++++++++++++++++++++++++++++++
void sigint(int sig) 
{/*{{{*/
	
	printf("End ejecution\n");
	final_close();
	exit(0);
	
}/*}}}*/
//+++++++++++++++++++++++++++++++++++++++++++++++ main thread ++++++++++++++++++++++++++++++++++++++++++
int main()
{/*{{{*/
	
	int err;
	signal(SIGINT, sigint);
	handle_record_device = ini_record_device() ;
	handle_playback_device = ini_playback_device() ;
	
	if( ( err = snd_pcm_prepare ( handle_record_device )  ) != 0 )
	{
		
		printf( "error in prepare record device , %d\n", err ) ;
		
	}
	
	if( ( err = snd_pcm_prepare ( handle_playback_device )  ) != 0 )
	{
		
		printf( "error in prepare play device , %d\n", err ) ;
		
	}
	
	while( !( SND_PCM_STATE_PREPARED == snd_pcm_state (handle_playback_device) ) )
	{
		
	}
	
	while( !( SND_PCM_STATE_PREPARED == snd_pcm_state (handle_record_device) ) )
	{
		
	}
	
	sleep(10);
	Capture_Playback(handle_record_device , handle_playback_device);
	
	printf("FIN");
	return 0;
	
}/*}}}*/
