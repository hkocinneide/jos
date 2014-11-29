#include <inc/lib.h>
#include <inc/jthread.h>

#define MATRIXSIZE 12
#define DEBUG 0
#define DEBUGDET 0 

int determ(int a[MATRIXSIZE][MATRIXSIZE], int n, bool thread);

int thread_determ(int a[MATRIXSIZE][MATRIXSIZE])
{
  if (DEBUGDET)
  {
    cprintf("matrix location passed: 0x%08x\n", a);
    cprintf("matrix passed to thread:\n");
  }
  int i, j;
  if (DEBUGDET)
  {
    for (i = 0; i < MATRIXSIZE-1; i++)
    {
      for (j = 0; j < MATRIXSIZE-1; j++)
      {
        cprintf("%d ", a[i][j]);
      }
      cprintf("\n");
    }
  }
  return determ(a, MATRIXSIZE - 1, false);
}

int determ(int a[MATRIXSIZE][MATRIXSIZE], int n, bool thread)
{
  int det = 0, jt = 0, p, h, k, i, j, temp[MATRIXSIZE][MATRIXSIZE];
  jthread_t tids[MATRIXSIZE];
  if (n == 1) 
  {
    return a[0][0];
  }
  if (n == 2)
  {
    return a[0][0] * a[1][1] - a[0][1] * a[1][0];
  }
  else
  {
    for (p = 0; p < n; p++)
    {
      h = 0;
      k = 0;
      for (i = 1; i < n; i++)
      {
        for (j = 0; j < n; j++)
        {
          if (j == p)
            continue;
          temp[h][k] = a[i][j];
          k++;
          if (k == n - 1)
          {
            h++;
            k = 0;
          }
        }
      }
      if (thread)
      {
        // int *thread_matrix = (int *)malloc((MATRIXSIZE - 1) * (MATRIXSIZE - 1) * sizeof(int));
        int (*thread_matrix)[MATRIXSIZE][MATRIXSIZE] = malloc(sizeof(int[MATRIXSIZE][MATRIXSIZE]));
        int ti, tj;
        if (DEBUGDET)
        {
          cprintf("Before threading:\ntemp matrix:\n");
          for (ti = 0; ti < MATRIXSIZE - 1; ti++)
          {
            for (tj = 0; tj < MATRIXSIZE - 1; tj++)
              cprintf("%d ", temp[ti][tj]);
            cprintf("\n");
          }
        }
        for (ti = 0; ti < MATRIXSIZE - 1; ti++)
        {
          for (tj = 0; tj < MATRIXSIZE - 1; tj++)
            (*thread_matrix)[ti][tj] = temp[ti][tj];
        }
        if (DEBUGDET)
        {
          cprintf("Copied matrix:\n");
          for (ti =0; ti < MATRIXSIZE - 1; ti++)
          {
            for(tj = 0 ; tj < MATRIXSIZE - 1; tj++)
              cprintf("%d ", (*thread_matrix)[ti][tj]);
            cprintf("\n");
          }
          cprintf("Copied matrix location: 0x%08x\n", thread_matrix);
        }
        jthread_create(&tids[jt++], NULL, (void *(*)(void *))thread_determ, (void *)thread_matrix);
        // int jt;
        // for (jt = 0; jt < MATRIXSIZE; jt++)
        // {
        //   jthread_create(&tids[jt], NULL, (void *(*)(void *))thread_determ, (void *)temp);
        // } 
        // for (jt = 0; jt < MATRIXSIZE; jt++)
        // {
        //   void *ret;
        //   jthread_join(tids[jt], &ret);
        //   det += a[0][(int)ret;
        // }
      }
      else
      {
        det = det + a[0][p]*(p % 2 ? -1 : 1) * determ(temp, n-1, false);
      }
    }
    if (thread)
    {
      for (jt = 0; jt < MATRIXSIZE; jt++)
      {
        void *ret;
        jthread_join(tids[jt], &ret);
        if (DEBUGDET)
        {
          cprintf("thread %d returns %d\n", jt, (int) ret);
          cprintf("%d + %d * %d * %d = ", det, a[0][jt], jt % 2 ? -1 : 1, (int)ret);
        }
        det += a[0][jt]*(jt % 2 ? -1 : 1) * ((int)ret);
        if (DEBUGDET)
          cprintf("%d\n", det);
      }
    }
    return det;
  }
}

void
umain(int argc, char *argv[])
{
  int i, j;
  int matrix[MATRIXSIZE][MATRIXSIZE];
  for (i = 0; i < MATRIXSIZE; i++)
  {
    for (j = 0; j < MATRIXSIZE; j++)
    {
      if (i  == 7  && j == 2)
      {
        matrix[i][j] = -8;
      }
      else
      {
        matrix[i][j] = (i * MATRIXSIZE) + j;
      }
      cprintf("%d ", matrix[i][j]);
    }
    cprintf("\n");
  }
  // while (i != 100)
  // {
  //   i++;
  //   determ(matrix, MATRIXSIZE);
  // }
  cprintf("\nSerial run:\n");
  unsigned tic = sys_time_msec();
  cprintf("det is: %d\n", determ(matrix, MATRIXSIZE, false));
  unsigned toc = sys_time_msec();
  cprintf("\n~~time the serial run took: %d msec~~\n\n", toc - tic);
  cprintf("Thread run:\n");
  tic = sys_time_msec();
  cprintf("det is: %d\n", determ(matrix, MATRIXSIZE, true));
  toc = sys_time_msec();
  cprintf("\n!!time the thread run took: %d msec!!\n\n", toc - tic);

  // for (i = 0; i < MATRIXSIZE * MATRIXSIZE; i++)
  //   Matrix[i] = i;
  // cprintf("\ndet is: %d\n\n", det(Matrix, 3));
}

// int *
// element(int *m, int x, int y)
// {
//   int pos = m - Matrix;
//   if (pos >= MATRIXSIZE * MATRIXSIZE || pos < 0)
//   {
//     cprintf("element: invalid matrix pointer\n");
//     return NULL;
//   }
//   int xoff = (pos % MATRIXSIZE);
//   int yoff = (pos - (pos % MATRIXSIZE)) / MATRIXSIZE;
//   int xnew = (xoff + x) % MATRIXSIZE;
//   int ynew = (yoff + y) % MATRIXSIZE;
//   pos = xnew + (ynew * MATRIXSIZE);
//   if (DEBUG)
//   {
//     cprintf("start pos: %d\n", pos);
//     cprintf("xnew: %d\n", xnew);
//     cprintf("ynew: %d\n", ynew);
//     cprintf("xoff: %d\n", xoff);
//     cprintf("yoff: %d\n", yoff);
//     cprintf("end pos: %d\n", pos);
//     cprintf("end value we point at: %d\n\n", Matrix[pos]);
//   }
//   return &Matrix[pos];
// }

// int det(int *m, int size)
// {
//   if (size == 0)
//   {
//     if (DEBUGDET)
//       cprintf("det: returning from size 0 matrix ERROR");
//     return 0;
//   }
//   if (size == 1)
//   {
//     if (DEBUGDET)
//       cprintf("det: size 1, det %d\n", *m);
//     return *m;
//   }
//   if (size == 2)
//   {
//     int zz = *element(m, 0, 0);
//     int oo = *element(m, 1, 1);
//     int zo = *element(m, 0, 1);
//     int oz = *element(m, 1, 0);
//     int r = (zz * oo) -
//             (zo * oz);
//     if (DEBUGDET)
//     {
//       cprintf("det: size 2, (%d * %d) - (%d * %d) = %d\n", zz, oo, zo, oz, r);
//     }
//     return r;
//   }
//   else
//   {
//     int i, d = 0;
//     for (i = 0; i < size; i++)
//     {
//       int head = *element(m, 0, i);
//       int rest = det(element(m, 1, (1 + i) % size), size - 1);
//       if (DEBUGDET)
//         cprintf("det: size > 2, head: %d, rest %d\n", head, rest);
//       d += head * rest;
//     }
//     return d;
//   }
// }
