#ifndef Event_included
#define Event_included

class Device;
class Volume;

class Event {

public:

    enum Type { None, Config,
		Insertion, Ejection, Suspend, Resume,
		Mount, Dismount, Export, Unexport, ForceUnmount};

    Event(Type t, Device *d, Volume *v)	: _type(t), _device(d), _volume(v) { }
    Type type() const			{ return _type; }
    Device *device() const		{ return _device; }
    Volume *volume() const		{ return _volume; }

private:

    Type _type;
    Device *_device;
    Volume *_volume;

};

#endif /* !Event_included */
