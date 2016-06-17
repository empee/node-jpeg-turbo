/*
 * This generates some test images into a RGBA buffer
 */
(function(def) {
  module.exports = def();
})(function() {
  var generator = {};

  // Put pixel in rgba array (unless x>width or x<0 or y>height or y<0)
  function putPixel(pixels, width, height, bpp, x, y, r, g, b) {
    if(x >= width || y >= height || x<0 || y<0) {
      return;
    }
    var pos = (y * width + x) * bpp;
    pixels[pos+0] = r;
    if(bpp>1) {
      pixels[pos+1] = g;
      pixels[pos+2] = b;
      pixels[pos+3] = 255;
    }
  }

  // Draws a horizontal or vertical line
  function drawLine(pixels, width, height, bpp, x, y, w, l, r, g, b) {
    for(var _y=y; _y<y+l; _y++) {
      for(var _x=x; _x<x+w; _x++) {
        putPixel(pixels, width, height, bpp, _x, _y, r, g, b);
      }
    }
  }

  // Draws a filled circle
  function fillCircle(pixels, width, height, bpp, x0, y0, radius, r, g, b) {
    var x = radius;
    var y = 0;
    var d = 1 - x;

    while (x >= y) {
      drawLine(pixels, width, height, bpp, -x + x0, y + y0, (x*2), 1, r, g, b);
      drawLine(pixels, width, height, bpp, -x + x0, -y + y0, (x*2), 1, r, g, b);
      drawLine(pixels, width, height, bpp, -y + x0, x + y0, (y*2), 1, r, g, b);
      drawLine(pixels, width, height, bpp, -y + x0, -x + y0, (y*2), 1, r, g, b);
      y++;
      if(d<=0) {
        d += 2 * y + 1;
      } else {
        x--;
        d += 2 * (y - x) + 1;
      }
    }
  }

  // Draws a "Jähne test pattern"
  generator.jahne = function(_color) {
    var color = typeof _color === 'undefined'?true:_color;
    var bpp = color ? 4 : 1;
    var pixels = Buffer.alloc(1024*1024*bpp);
    var width = 1024, height = 1024;
    var x0 = 0, y0 = 0;
    var f = 0.5;

    var x, y, dx, dy, o, r, g, b, centerX = width / 2, centerY = height / 2;

    for(y=0; y<=centerY; y++) {
      dy = y - centerY;
      for(x=0;x<=centerX;x++) {
        dx = x - centerX;
        o = f * ((dx*dx / width) + (dy*dy / height));
        r = g = b = (127 * (Math.sin(o * (Math.PI)) + 1))|0;
        if(color) {
          g = (127 * (Math.sin(o * (Math.PI*2) + 2*Math.PI/3) + 1))|0;
          b = (127 * (Math.sin(o * (Math.PI*4) + 4*Math.PI/3) + 1))|0;
        }
        putPixel(pixels, width, height, bpp, x0 + x, y0 + y, r, g, b);
        putPixel(pixels, width, height, bpp, x0 + width - x, y0 + y, r, g, b);
        putPixel(pixels, width, height, bpp, x0 + width - x, y0 + height - y, r, g, b);
        putPixel(pixels, width, height, bpp, x0 + x, y0 + height - y, r, g, b);
      }
    }
    return pixels.slice(0, 1024*1024*bpp);
  };

  // Encodes xPosition and yPosition withing the colors
  function encodePos(pixels, width, height, bpp, x0, y0, w, h, xPosition, yPosition) {
    // 2 blocks, one to hold xPosition and other for yPosition
    var blockWidth = w / 2;
    // 3 sub blocks to hold the coordinate in different ways, so maybe one will survive compression
    var subBlockHeight = h / 3;
    var c = xPosition;
    for(var x = x0; x < x0 + (blockWidth*2); x+=blockWidth) {
      var subBlock = 0;
      for(var y = y0; y <= y0 + (subBlockHeight*2); y+=subBlockHeight) {
        if(subBlock === 0) {
          drawLine(pixels, width, height, bpp, x, y, blockWidth, subBlockHeight, c*2, c, c*2);
        } else if(subBlock === 1) {
          drawLine(pixels, width, height, bpp, x, y, blockWidth, subBlockHeight, c*4, c, c*4);
        } else if(subBlock === 2) {
          drawLine(pixels, width, height, bpp, x, y, blockWidth, subBlockHeight, c*6, c, c*6);
        }
        subBlock++;
      }
      c = yPosition;
    }
  }

  // Draws a crop test image, 60x60 squares with
  // This will survive compression quality 85 and still work as intented as long as sampling is 444
  generator.crop = function() {
    var pixels = Buffer.alloc(1024*1024*4);
    var width = 1024, height = 1024
    var bpp = 4; // Supports only rgba

    var x, y;

    // draw vertical edges of squares
    for(x = 0; x <= width + width%60; x+=60) {
      drawLine(pixels, width, height, bpp, x-4, 0, 4, height, 96, 96, 96); // right edge
      drawLine(pixels, width, height, bpp, x, 0, 4, height, 112, 112, 112); // left edge
    }

    // draw horizontal edges of squares
    for(y = 0; y <= height + height%60; y+=60) {
      drawLine(pixels, width, height, bpp, 0, y-4, width, 4, 96, 96, 96); // bottom edge
      drawLine(pixels, width, height, bpp, 0, y, width, 4, 112, 112, 112); // top edge
    }

    // encode position data for each square
    for(y = 0; y <= height + height%60; y+=60) {
      for(x = 0; x <= width + width%60; x+=60) {
        encodePos(pixels, width, height, bpp, x+6, y+6, 48, 48, (x/60), (y/60));
      }
    }

    // draw nice dots to look at if you actually open the image + add the locating pixel
    for(y = 0; y <= height + height%60; y+=60) {
      for(x = 0; x <= width + width%60; x+=60) {
        fillCircle(pixels, width, height, bpp, x, y, 6, 255, 255, 255);
        putPixel(pixels, width, height, bpp, x+4, y+4, 0, 0, 255); // locating pixel
      }
    }

    return pixels.slice(0, 1024*1024*4);
  };

  function findMostCommon(arr) {
    var i = {};
    arr.forEach(function(coord) {
      i[coord.r] = !i[coord.r]?1:i[coord.r]+1;
      i[coord.g] = !i[coord.g]?1:i[coord.g]+1;
      i[coord.b] = !i[coord.b]?1:i[coord.b]+1;
    });
    var ret = null;
    Object.keys(i).forEach(function(key) {
      if(!ret || i[ret] < i[key]) {
        ret = key;
      }
    });
    return ret?parseInt(ret):ret;
  }

  function parseCoordinate(data, width, height, bpp, x0, y0) {
    var d = {
      l: [],
      m: [],
      h: []
    };
    for(var y=y0; y<y0+48; y++) {
      if(y>=height) {
        break;
      }
      for(var x=x0; x<x0+24; x++) {
        if(x>=width) {
          break;
        }
        var pos = (y*width+x) * bpp;
        if(y-y0 < 16) {
          d.l.push({r: Math.round(data[pos+0]/2), g: data[pos+1], b: Math.round(data[pos+2]/2)});
        }
        else if(y-y0 < 32) {
          d.m.push({r: Math.round(data[pos+0]/4), g: data[pos+1], b: Math.round(data[pos+2]/4)});
        }
        else {
          d.h.push({r: Math.round(data[pos+0]/6), g: data[pos+1], b: Math.round(data[pos+2]/6)});
        }
      }
    }
    if (d.l.length === 0) {
      return -1;
    }
    var l = findMostCommon(d.l);
    var m = findMostCommon(d.m);
    var h = findMostCommon(d.h);

    if(l == m || l === h) {
      return l;
    }
    else if(m === h) {
      return m;
    }
    else {
      if(h) {
        return h;
      }
      else if(m) {
        return m;
      }
      return l;
    }
  }

  function parseCoordinates(data, width, height, bpp, x, y) {
    var ret = {
      x: parseCoordinate(data, width, height, bpp, x+2, y+2),
      y: parseCoordinate(data, width, height, bpp, x+26, y+2)
    };
    return ret;
  }

  function findVal(arr, mode, offset) {
    var val = null;
    var keys = Object.keys(arr);
    if(mode === 'max') {
      keys.reverse();
    }
    keys = keys.slice(0, 15);
    keys.forEach(function(x) {
      if(mode === 'max') {
        if(val === null || (arr[x] > 150 && arr[x] > arr[val]) ) {
          val = x;
        }
      } else {
        //console.log(val, x, arr[x], arr[val]);
        if(val === null || (arr[x] > 150 && arr[x] > arr[val]) ) {
          val = x;
        }
      }
    });
    return val?parseInt(val)*4 + (mode==='max'?4:0) + offset:null;
  }

  function checkPos(data, pos, min, max) {
    if(!data[pos + 0] || data[pos + 0] < min || data[pos + 0] > max) {
      return false;
    }
    if(!data[pos + 1] || data[pos + 1] < min || data[pos + 1] > max) {
      return false;
    }
    if(!data[pos + 2] || data[pos + 2] < min || data[pos + 2] > max) {
      return false;
    }
    return true;
  }

  function findEdges(data, width, height, bpp, topLeft) {
    var yStart = topLeft.y - 4;
    var xStart = topLeft.x - 4;
    if(yStart < 0) {
      yStart += 60;
    }
    if(xStart < 0) {
      xStart += 60;
    }

    var vStart = {}, vEnd = {}, hStart = {}, hEnd = {};

    for(var y=yStart, y2=0, yIndex=0; y<height; y++, y2++) {
      for(var x=xStart, x2=0, xIndex=0; x<width; x++, x2++) {
        var pos = (y*width+x)*bpp;
        if(checkPos(data, pos, 106, 126)) {
          hStart[xIndex] = hStart[xIndex]?hStart[xIndex]+1:1;
          vStart[yIndex] = vStart[yIndex]?vStart[yIndex]+1:1;
        }
        if(checkPos(data, pos, 85, 105)) {
          hEnd[xIndex] = hEnd[xIndex]?hEnd[xIndex]+1:1;
          vEnd[yIndex] = vEnd[yIndex]?vEnd[yIndex]+1:1;
        }
        if(x2%4 === 0 && x2!==0) {
          xIndex++;
        }
      }
      if(y2%4 === 0 && y2!==0) {
        yIndex++;
      }
    }

    var ret = {
      top: findVal(vStart, 'min', yStart),
      bottom: findVal(vEnd, 'max', yStart),
      left: findVal(hStart, 'min', xStart),
      right: findVal(hEnd, 'max', xStart)
    };

    return ret;
  }

  generator.findCoordinates = function(data, width, height, bpp) {
    var ret = { blocks: {x: [], y: []} };
    var x, y, rows=[], cols=[], escape, colsDone, rowDone;

    var topLeft = {x: -1, y: -1};

    for(y=0;y<height; y++) {
      for(x=0;x<width; x++) {
        var pos = (y*width+x) * bpp;

        if(data[pos] <= 60 && data[pos+1] <= 60 && data[pos+2] >= 200) {
          if(topLeft.x === -1) {
            topLeft.x = x;
            topLeft.y = y;
          }
        }
      }
    }

    if(topLeft.x === -1) {
      return ret;
    }

    var edges = findEdges(data, width, height, bpp, topLeft);
    ret.left = edges.left === null?undefined:edges.left;
    ret.right = edges.right === null?undefined:(width-edges.right);
    ret.top = edges.top === null?undefined:edges.top;
    ret.bottom = edges.bottom === null?undefined:(height-edges.bottom);

    for(y=topLeft.y, escape = false, colsDone = false; y<=height - ret.bottom; y+= 60) {
      if(y + 56 > height) {
        break;
      }
      for(x=topLeft.x, rowDone = false; x<=width - ret.right; x+= 60) {
        if(x + 56 > width) {
          break;
        }
        var coord = parseCoordinates(data, width, height, bpp, x, y);
        if(coord.x === -1 || coord.y === -1) {
          escape = true;
          break;
        }

        if(!colsDone) {
          if(cols.indexOf(coord.x) !== -1) {
            escape = true;
            break;
          }
          cols.push(coord.x);
        } else if(cols.indexOf(coord.x) === -1) {
          escape = true;
          break;
        }

        if(!rowDone) {
          if(rows.indexOf(coord.y) !== -1) {
            escape = true;
            break;
          }
          rows.push(coord.y);
          rowDone = true;
        } else if(rows.indexOf(coord.y) === -1) {
          escape = true;
          break;
        }
      }
      colsDone = true;
      if(escape) {
        break;
      }
    }
    ret.blocks.y = [rows[0], rows[rows.length-1]];
    ret.blocks.x = [cols[0], cols[cols.length-1]];

    return ret;
  };

  return generator;
});
