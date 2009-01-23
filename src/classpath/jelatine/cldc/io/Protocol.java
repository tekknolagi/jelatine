package jelatine.cldc.io;

import java.io.IOException;

import javax.microedition.io.Connection;

public interface Protocol {
    
    public Connection open(URL url, int mode, boolean timeouts) throws IllegalArgumentException, IOException;

}
