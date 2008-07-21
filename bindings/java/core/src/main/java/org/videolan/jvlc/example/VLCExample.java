package org.videolan.jvlc.example;

import org.videolan.jvlc.Audio;
import org.videolan.jvlc.JVLC;
import org.videolan.jvlc.MediaDescriptor;
import org.videolan.jvlc.MediaPlayer;
import org.videolan.jvlc.Video;
import org.videolan.jvlc.event.MediaPlayerListener;


public class VLCExample
{

    public static void main(String[] args) throws InterruptedException
    {
        System.out.println("== Starting VLCExample ==");
        if (args.length == 0)
        {
            System.out.print("Creating a JVLC instance without args");
        }
        else
        {
            System.out.println("Creating a JVLC instance with args: ");
            for (int i = 0; i < args.length; i++)
            {
                System.out.println(i + ") " + args[i]);
            }
        }
        JVLC jvlc = new JVLC(args);
        System.out.println("... done.");

        MediaDescriptor mediaDescriptor = new MediaDescriptor(jvlc, "/home/carone/apps/a.avi");
        MediaPlayer mediaInstance = mediaDescriptor.getMediaInstance();

        mediaInstance.addListener(new MediaPlayerListener()
        {

            public void endReached(MediaPlayer mediaInstance)
            {
                System.out.println("Media instance end reached. MRL: " + mediaInstance.getMediaDescriptor().getMrl());
            }

            public void paused(MediaPlayer mediaInstance)
            {
                System.out.println("Media instance paused. MRL: " + mediaInstance.getMediaDescriptor().getMrl());
            }

            public void played(MediaPlayer mediaInstance)
            {
                System.out.println("Media instance played. MRL: " + mediaInstance.getMediaDescriptor().getMrl());
            }

            public void positionChanged(MediaPlayer mediaInstance)
            {
                // TODO Auto-generated method stub
            }

            public void timeChanged(MediaPlayer mediaInstance, long newTime)
            {
                System.out.println("new time: " + newTime);
            }
        });
        mediaInstance.play();

        while (!mediaInstance.hasVideoOutput())
        {
            Thread.sleep(100);
        }

        Video video = new Video(jvlc);
        System.out.print(video.getWidth(mediaInstance));
        System.out.print("x");
        System.out.println(video.getHeight(mediaInstance));
        System.out.print("Fullscreen... ");
        video.setFullscreen(mediaInstance, true);
        Thread.sleep(3000);
        System.out.println("real size.");
        video.setFullscreen(mediaInstance, false);
        System.out.print("Taking snapshot... ");
        video.getSnapshot(mediaInstance, System.getProperty("user.dir") + "/snap.png", 0, 0);
        System.out.println("taken. (see " + System.getProperty("user.dir") + "/snap.png )");
        Thread.sleep(2000);
        System.out.println("Resizing to 300x300");
        video.setSize(300, 300);

        System.out.print("Muting...");
        Audio audio = new Audio(jvlc);
        audio.setMute(true);
        Thread.sleep(3000);
        System.out.println("unmuting.");
        audio.setMute(false);
        Thread.sleep(3000);
        System.out.println("Volume is: " + audio.getVolume());
        System.out.print("Setting volume to 150... ");
        audio.setVolume(150);
        System.out.println("done");
        System.out.println("== AUDIO INFO ==");
        System.out.println("Audio track number: " + audio.getTrack(mediaInstance));
        System.out.println("Audio channel info: " + audio.getChannel());
        Thread.sleep(3000);
        System.out.println("MEDIA INSTANCE INFORMATION");
        System.out.println("--------------------------");
        System.out.println("Total length (ms) :\t" + mediaInstance.getLength());
        System.out.println("Input time (ms) :\t" + mediaInstance.getTime());
        System.out.println("Input position [0-1]:\t" + mediaInstance.getPosition());
        System.out.println("Input FPS :\t" + mediaInstance.getFPS());

        System.out.println("Everything fine ;)");
        return;
    }
}
