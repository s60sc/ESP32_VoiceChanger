
      // derived from https://codepen.io/willat600series/pen/ojzYJx
      function LogSlider(inminpos, inmaxpos) {
        minpos =  inminpos;
        maxpos =  inmaxpos;
        minval = Math.log(minpos);
        maxval = Math.log(maxpos);
        scale = (maxval - minval) / (maxpos - minpos);
      }

      LogSlider.prototype = {
        // Calculate value from a slider position
        value: function (position) {
          return Math.exp((position - minpos) * scale + minval);
        },
        // Calculate slider position from a value
        position: function (value) {
          return minpos + (Math.log(value) - minval) / scale;
        } 
      };
      
      var logQ = new LogSlider(0.1, 20);

      $('#Qval').on('change', function () {
        var val = logQ.value(+$(this).val());
        $('#QvalT').val(val.toFixed(1)); 
      });
            
      var logSn = new LogSlider(1, 2000);

      $('#SineFreq').on('change', function () {
        var val = logSn.value(+$(this).val());
        $('#SineFreq').val(val()); 
      });
