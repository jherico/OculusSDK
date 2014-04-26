package com.oculusvr.capi;

import java.util.Arrays;
import java.util.List;

import com.sun.jna.Pointer;
import com.sun.jna.Structure;

public class Matrix4f extends Structure {
  public float[] M = new float[((4) * (4))];

  public Matrix4f() {
    super();
  }

  @Override
  protected List<?> getFieldOrder() {
    return Arrays.asList("M");
  }

  public Matrix4f(float M[]) {
    super();
    if ((M.length != this.M.length))
      throw new IllegalArgumentException("Wrong array size !");
    this.M = M;
  }

  public Matrix4f(Pointer peer) {
    super(peer);
  }

  public static class ByReference extends Matrix4f implements Structure.ByReference {

  };

  public static class ByValue extends Matrix4f implements Structure.ByValue {

  };

  public org.saintandreas.math.Matrix4f toMatrix4f() {
    return new org.saintandreas.math.Matrix4f(M).transpose();
  }
}
