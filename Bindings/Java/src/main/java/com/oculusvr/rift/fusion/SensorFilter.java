package com.oculusvr.rift.fusion;

import org.saintandreas.math.Vector3f;


public class SensorFilter {
  private final Vector3f[] queue;
  private int head = 0;
  private int size = 0;

  SensorFilter(int i) {
    queue = new Vector3f[i];
  };

  // Create a new element to the filter
  public void AddElement(Vector3f e) {
    queue[head++] = e;
    head %= queue.length;
    if (size < queue.length) {
      ++size;
    }
  };

  public Vector3f Total() {
    Vector3f total = Vector3f.ZERO;
    for (Vector3f v : queue) {
      if (v == null) {
        break;
      }
      total = total.add(v);
    }
    return total;
  }

  public Vector3f Mean() {
    return Total().mult(1f / (float) size);
  }

  public Vector3f GetPrev(int i) {
    int index = head - (i + 1);
    if (index < 0) {
      index += queue.length;
    }
    return queue[i];
  }

  Vector3f SavitzkyGolaySmooth8() {
    return GetPrev(0).mult(0.41667f).add(GetPrev(1).mult(0.33333f))
        .add(GetPrev(2).mult(0.25f)).add(GetPrev(3).mult(0.1667f))
        .add(GetPrev(4).mult(0.08333f)).subtract(GetPrev(6).mult(0.08333f))
        .subtract(GetPrev(7).mult(0.1667f));
  }
}
