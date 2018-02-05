README
======

Compiling and installing
========================

The source uses GNU Autotools (autoconf_, automake_, libtool_), so
compiling and installing is simple:

.. code-block:: shell

    $ ./configure AR=gcc-ar NM=gcc-nm RANLIB=gcc-ranlib
    $ make
    $ make install

.. _autoconf: http://www.gnu.org/software/autoconf/
.. _automake: http://www.gnu.org/software/automake/
.. _libtool: http://www.gnu.org/software/libtool/
