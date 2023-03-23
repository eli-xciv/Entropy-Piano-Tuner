# Entropy Piano Tuner

## Information
For general information about the software have a look at the [homepage](http://piano-tuner.org/) of the project.

## Building

```
# Build EPT

# Apply Patch
patch -N1 -i qwt.patch

# Install RPM deps
sudo dnf install -y qt-dev qt-devel /usr/bin/qmake qt5-devel qt5-qtbase-devel qt5-qtbase qt5-qtbase-common qt5-qtbase-gui qt5-qtbase-static qt5-qtmultimedia-devel

# build qtmidi
qmake-qt5
RPM_ARCH="x86_64" RPM_PACKAGE_RELEASE="flatpak" RPM_PACKAGE_VERSION="0.0.1-alpha" RPM_PACKAGE_NAME="ept-flatpak" make

# build all other third party
cd thirdparty
qmake-qt5
make

cd ..
qmake-qt5
make
```

### Quick instructions
The fundamental workflow to complie the Entropy Piano Tuner is the following. For further information regarding your platform have a look in the [developer pages](http://develop.piano-tuner.org).

* Install [Qt](https://www.qt.io/download-open-source/) for your platform.
* Follow the instructions of [qtmidi](https://gitlab.com/tp3/qtmidi) to build the required midi plugin.
* Clone the full repositoy including all submodules via `git clone --recursive https://gitlab.com/tp3/Entropy-Piano-Tuner.git`.
* Open the `entropypianotuner.pro` file using the QtCreator to build and run the project. Alternatively you can use `qmake` to build from the console.


### Developer pages
Regarding development please have a look at the [developer pages](http://develop.piano-tuner.org) for further information.
