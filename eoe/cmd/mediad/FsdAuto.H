#ifndef Fsd_defined
#define Fsd_defined

class Fsd {
    
public:

    const mntent& operator [] (unsigned int) const;
          mntent& operator [] (unsigned int);

    static const Fsd& auto();
    static Fsd& tab();

private:

    FAMonitor(

};

#endif /* !Fsd_defined */
