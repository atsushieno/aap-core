/*
 * This file is auto-generated.  DO NOT MODIFY.
 */
package org.androidaudioplugin;
public interface AudioPluginInterface extends android.os.IInterface
{
  /** Default implementation for AudioPluginInterface. */
  public static class Default implements org.androidaudioplugin.AudioPluginInterface
  {
    @Override public int beginCreate(java.lang.String pluginId, int sampleRate) throws android.os.RemoteException
    {
      return 0;
    }
    @Override public void addExtension(int instanceID, java.lang.String uri, android.os.ParcelFileDescriptor sharedMemoryFD, int size) throws android.os.RemoteException
    {
    }
    @Override public void endCreate(int instanceID) throws android.os.RemoteException
    {
    }
    @Override public boolean isPluginAlive(int instanceID) throws android.os.RemoteException
    {
      return false;
    }
    @Override public int getStateSize(int instanceID) throws android.os.RemoteException
    {
      return 0;
    }
    @Override public void getState(int instanceID, android.os.ParcelFileDescriptor sharedMemoryFD) throws android.os.RemoteException
    {
    }
    @Override public void setState(int instanceID, android.os.ParcelFileDescriptor sharedMemoryFD, int size) throws android.os.RemoteException
    {
    }
    @Override public void prepare(int instanceID, int frameCount, int portCount) throws android.os.RemoteException
    {
    }
    @Override public void prepareMemory(int instanceID, int shmFDIndex, android.os.ParcelFileDescriptor sharedMemoryFD) throws android.os.RemoteException
    {
    }
    @Override public void activate(int instanceID) throws android.os.RemoteException
    {
    }
    @Override public void process(int instanceID, int timeoutInNanoseconds) throws android.os.RemoteException
    {
    }
    @Override public void deactivate(int instanceID) throws android.os.RemoteException
    {
    }
    @Override public void destroy(int instanceID) throws android.os.RemoteException
    {
    }
    @Override
    public android.os.IBinder asBinder() {
      return null;
    }
  }
  /** Local-side IPC implementation stub class. */
  public static abstract class Stub extends android.os.Binder implements org.androidaudioplugin.AudioPluginInterface
  {
    /** Construct the stub at attach it to the interface. */
    public Stub()
    {
      this.attachInterface(this, DESCRIPTOR);
    }
    /**
     * Cast an IBinder object into an org.androidaudioplugin.AudioPluginInterface interface,
     * generating a proxy if needed.
     */
    public static org.androidaudioplugin.AudioPluginInterface asInterface(android.os.IBinder obj)
    {
      if ((obj==null)) {
        return null;
      }
      android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
      if (((iin!=null)&&(iin instanceof org.androidaudioplugin.AudioPluginInterface))) {
        return ((org.androidaudioplugin.AudioPluginInterface)iin);
      }
      return new org.androidaudioplugin.AudioPluginInterface.Stub.Proxy(obj);
    }
    @Override public android.os.IBinder asBinder()
    {
      return this;
    }
    @Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
    {
      java.lang.String descriptor = DESCRIPTOR;
      switch (code)
      {
        case INTERFACE_TRANSACTION:
        {
          reply.writeString(descriptor);
          return true;
        }
      }
      switch (code)
      {
        case TRANSACTION_beginCreate:
        {
          data.enforceInterface(descriptor);
          java.lang.String _arg0;
          _arg0 = data.readString();
          int _arg1;
          _arg1 = data.readInt();
          int _result = this.beginCreate(_arg0, _arg1);
          reply.writeNoException();
          reply.writeInt(_result);
          return true;
        }
        case TRANSACTION_addExtension:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          java.lang.String _arg1;
          _arg1 = data.readString();
          android.os.ParcelFileDescriptor _arg2;
          if ((0!=data.readInt())) {
            _arg2 = android.os.ParcelFileDescriptor.CREATOR.createFromParcel(data);
          }
          else {
            _arg2 = null;
          }
          int _arg3;
          _arg3 = data.readInt();
          this.addExtension(_arg0, _arg1, _arg2, _arg3);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_endCreate:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          this.endCreate(_arg0);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_isPluginAlive:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          boolean _result = this.isPluginAlive(_arg0);
          reply.writeNoException();
          reply.writeInt(((_result)?(1):(0)));
          return true;
        }
        case TRANSACTION_getStateSize:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _result = this.getStateSize(_arg0);
          reply.writeNoException();
          reply.writeInt(_result);
          return true;
        }
        case TRANSACTION_getState:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          android.os.ParcelFileDescriptor _arg1;
          if ((0!=data.readInt())) {
            _arg1 = android.os.ParcelFileDescriptor.CREATOR.createFromParcel(data);
          }
          else {
            _arg1 = null;
          }
          this.getState(_arg0, _arg1);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_setState:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          android.os.ParcelFileDescriptor _arg1;
          if ((0!=data.readInt())) {
            _arg1 = android.os.ParcelFileDescriptor.CREATOR.createFromParcel(data);
          }
          else {
            _arg1 = null;
          }
          int _arg2;
          _arg2 = data.readInt();
          this.setState(_arg0, _arg1, _arg2);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_prepare:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          int _arg2;
          _arg2 = data.readInt();
          this.prepare(_arg0, _arg1, _arg2);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_prepareMemory:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          android.os.ParcelFileDescriptor _arg2;
          if ((0!=data.readInt())) {
            _arg2 = android.os.ParcelFileDescriptor.CREATOR.createFromParcel(data);
          }
          else {
            _arg2 = null;
          }
          this.prepareMemory(_arg0, _arg1, _arg2);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_activate:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          this.activate(_arg0);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_process:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          int _arg1;
          _arg1 = data.readInt();
          this.process(_arg0, _arg1);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_deactivate:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          this.deactivate(_arg0);
          reply.writeNoException();
          return true;
        }
        case TRANSACTION_destroy:
        {
          data.enforceInterface(descriptor);
          int _arg0;
          _arg0 = data.readInt();
          this.destroy(_arg0);
          reply.writeNoException();
          return true;
        }
        default:
        {
          return super.onTransact(code, data, reply, flags);
        }
      }
    }
    private static class Proxy implements org.androidaudioplugin.AudioPluginInterface
    {
      private android.os.IBinder mRemote;
      Proxy(android.os.IBinder remote)
      {
        mRemote = remote;
      }
      @Override public android.os.IBinder asBinder()
      {
        return mRemote;
      }
      public java.lang.String getInterfaceDescriptor()
      {
        return DESCRIPTOR;
      }
      @Override public int beginCreate(java.lang.String pluginId, int sampleRate) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeString(pluginId);
          _data.writeInt(sampleRate);
          boolean _status = mRemote.transact(Stub.TRANSACTION_beginCreate, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              return getDefaultImpl().beginCreate(pluginId, sampleRate);
            }
          }
          _reply.readException();
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      @Override public void addExtension(int instanceID, java.lang.String uri, android.os.ParcelFileDescriptor sharedMemoryFD, int size) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          _data.writeString(uri);
          if ((sharedMemoryFD!=null)) {
            _data.writeInt(1);
            sharedMemoryFD.writeToParcel(_data, 0);
          }
          else {
            _data.writeInt(0);
          }
          _data.writeInt(size);
          boolean _status = mRemote.transact(Stub.TRANSACTION_addExtension, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().addExtension(instanceID, uri, sharedMemoryFD, size);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void endCreate(int instanceID) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          boolean _status = mRemote.transact(Stub.TRANSACTION_endCreate, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().endCreate(instanceID);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public boolean isPluginAlive(int instanceID) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        boolean _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          boolean _status = mRemote.transact(Stub.TRANSACTION_isPluginAlive, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              return getDefaultImpl().isPluginAlive(instanceID);
            }
          }
          _reply.readException();
          _result = (0!=_reply.readInt());
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      @Override public int getStateSize(int instanceID) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        int _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          boolean _status = mRemote.transact(Stub.TRANSACTION_getStateSize, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              return getDefaultImpl().getStateSize(instanceID);
            }
          }
          _reply.readException();
          _result = _reply.readInt();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
      @Override public void getState(int instanceID, android.os.ParcelFileDescriptor sharedMemoryFD) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          if ((sharedMemoryFD!=null)) {
            _data.writeInt(1);
            sharedMemoryFD.writeToParcel(_data, 0);
          }
          else {
            _data.writeInt(0);
          }
          boolean _status = mRemote.transact(Stub.TRANSACTION_getState, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().getState(instanceID, sharedMemoryFD);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void setState(int instanceID, android.os.ParcelFileDescriptor sharedMemoryFD, int size) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          if ((sharedMemoryFD!=null)) {
            _data.writeInt(1);
            sharedMemoryFD.writeToParcel(_data, 0);
          }
          else {
            _data.writeInt(0);
          }
          _data.writeInt(size);
          boolean _status = mRemote.transact(Stub.TRANSACTION_setState, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().setState(instanceID, sharedMemoryFD, size);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void prepare(int instanceID, int frameCount, int portCount) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          _data.writeInt(frameCount);
          _data.writeInt(portCount);
          boolean _status = mRemote.transact(Stub.TRANSACTION_prepare, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().prepare(instanceID, frameCount, portCount);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void prepareMemory(int instanceID, int shmFDIndex, android.os.ParcelFileDescriptor sharedMemoryFD) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          _data.writeInt(shmFDIndex);
          if ((sharedMemoryFD!=null)) {
            _data.writeInt(1);
            sharedMemoryFD.writeToParcel(_data, 0);
          }
          else {
            _data.writeInt(0);
          }
          boolean _status = mRemote.transact(Stub.TRANSACTION_prepareMemory, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().prepareMemory(instanceID, shmFDIndex, sharedMemoryFD);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void activate(int instanceID) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          boolean _status = mRemote.transact(Stub.TRANSACTION_activate, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().activate(instanceID);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void process(int instanceID, int timeoutInNanoseconds) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          _data.writeInt(timeoutInNanoseconds);
          boolean _status = mRemote.transact(Stub.TRANSACTION_process, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().process(instanceID, timeoutInNanoseconds);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void deactivate(int instanceID) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          boolean _status = mRemote.transact(Stub.TRANSACTION_deactivate, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().deactivate(instanceID);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void destroy(int instanceID) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeInt(instanceID);
          boolean _status = mRemote.transact(Stub.TRANSACTION_destroy, _data, _reply, 0);
          if (!_status) {
            if (getDefaultImpl() != null) {
              getDefaultImpl().destroy(instanceID);
              return;
            }
          }
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      public static org.androidaudioplugin.AudioPluginInterface sDefaultImpl;
    }
    static final int TRANSACTION_beginCreate = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
    static final int TRANSACTION_addExtension = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
    static final int TRANSACTION_endCreate = (android.os.IBinder.FIRST_CALL_TRANSACTION + 2);
    static final int TRANSACTION_isPluginAlive = (android.os.IBinder.FIRST_CALL_TRANSACTION + 3);
    static final int TRANSACTION_getStateSize = (android.os.IBinder.FIRST_CALL_TRANSACTION + 4);
    static final int TRANSACTION_getState = (android.os.IBinder.FIRST_CALL_TRANSACTION + 5);
    static final int TRANSACTION_setState = (android.os.IBinder.FIRST_CALL_TRANSACTION + 6);
    static final int TRANSACTION_prepare = (android.os.IBinder.FIRST_CALL_TRANSACTION + 7);
    static final int TRANSACTION_prepareMemory = (android.os.IBinder.FIRST_CALL_TRANSACTION + 8);
    static final int TRANSACTION_activate = (android.os.IBinder.FIRST_CALL_TRANSACTION + 9);
    static final int TRANSACTION_process = (android.os.IBinder.FIRST_CALL_TRANSACTION + 10);
    static final int TRANSACTION_deactivate = (android.os.IBinder.FIRST_CALL_TRANSACTION + 11);
    static final int TRANSACTION_destroy = (android.os.IBinder.FIRST_CALL_TRANSACTION + 12);
    public static boolean setDefaultImpl(org.androidaudioplugin.AudioPluginInterface impl) {
      // Only one user of this interface can use this function
      // at a time. This is a heuristic to detect if two different
      // users in the same process use this function.
      if (Stub.Proxy.sDefaultImpl != null) {
        throw new IllegalStateException("setDefaultImpl() called twice");
      }
      if (impl != null) {
        Stub.Proxy.sDefaultImpl = impl;
        return true;
      }
      return false;
    }
    public static org.androidaudioplugin.AudioPluginInterface getDefaultImpl() {
      return Stub.Proxy.sDefaultImpl;
    }
  }
  public static final java.lang.String DESCRIPTOR = "org.androidaudioplugin.AudioPluginInterface";
  public int beginCreate(java.lang.String pluginId, int sampleRate) throws android.os.RemoteException;
  public void addExtension(int instanceID, java.lang.String uri, android.os.ParcelFileDescriptor sharedMemoryFD, int size) throws android.os.RemoteException;
  public void endCreate(int instanceID) throws android.os.RemoteException;
  public boolean isPluginAlive(int instanceID) throws android.os.RemoteException;
  public int getStateSize(int instanceID) throws android.os.RemoteException;
  public void getState(int instanceID, android.os.ParcelFileDescriptor sharedMemoryFD) throws android.os.RemoteException;
  public void setState(int instanceID, android.os.ParcelFileDescriptor sharedMemoryFD, int size) throws android.os.RemoteException;
  public void prepare(int instanceID, int frameCount, int portCount) throws android.os.RemoteException;
  public void prepareMemory(int instanceID, int shmFDIndex, android.os.ParcelFileDescriptor sharedMemoryFD) throws android.os.RemoteException;
  public void activate(int instanceID) throws android.os.RemoteException;
  public void process(int instanceID, int timeoutInNanoseconds) throws android.os.RemoteException;
  public void deactivate(int instanceID) throws android.os.RemoteException;
  public void destroy(int instanceID) throws android.os.RemoteException;
}
