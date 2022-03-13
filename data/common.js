 
      var baseHost = window.location.origin
      var myUrl = baseHost +"/";
      // initialise / update on page load
      $(function() { 
        // update page content using received JSON
        $.ajax({ // send request to app
          url: myUrl+"status",           
          dataType : "json", 
          success: function(data) { // receive response from app
            $.each(data, function(key, val) { 
              // replace each existing value with new value, using key name to match html tag id
              $('#'+key).text(val);
              $('#'+key).val(val);
              $('input[type="radio"][name="'+key+'"][value="'+val+'"]').prop("checked", true);
              $('input[type="checkbox"][id="'+key+'"]').prop("checked", parseInt(val, 10));
              $('option[name="'+key+'"][value="'+val+'"]').prop("selected", true);
            });
          },
          error: function(xhr, status, error) {
            console.log("Failed to get data: " + xhr.responseText); 
            console.log("Status: " + status);
          }  
        });
      });
      
      function sendUpdates(doAction) {    
        // get each input field and obtain id/name and value into array to send as json
        var jarray = {};
        jarray["action"] = doAction;
        $('input').each(function () {
          if ($(this).attr('type') == "text") jarray[$(this).attr('id')] = $(this).val().trim();
          if ($(this).attr('type') == "password") jarray[$(this).attr('id')] = $(this).val().trim();
          if ($(this).attr('type') == "range")jarray[$(this).attr('id')] = $(this).val().trim();
          // for radio fields return value of radio button that is selected
          if ($(this).attr('type') == "radio" && $(this).is(":checked")) 
            jarray[$(this).attr('name')] = $(this).val(); 
          // for checkboxes set return to 1 if checked else 0
          if ($(this).attr('type') == "checkbox")
            jarray[$(this).attr('id')] = $(this).is(":checked") ? "1" : "0";
        });
        $('option').each(function() {
          if ($(this).is(":selected")) jarray[$(this).attr('name')] = $(this).val(); 
        });

        $.ajax({
          url : '/update',
          type : 'POST',
          contentType: "application/json",
          data : JSON.stringify(jarray),
          error: function(xhr, status, error) {
            console.log("Failed to send data: " + xhr.responseText); 
            console.log("Status: " + status);
          } 
        }); 
      }
      
      $('input[type="range"]').on('input', function () {
        // determine range slider floating value position from value
        var control = $(this),
        controlMin = control.attr('min'),
        controlMax = control.attr('max'),
        controlVal = control.val();

        var range = controlMax - controlMin;
        var position = (controlVal - controlMin) / range * 30;
        
        var output = control.next('output');
        output.css('left', 'calc(' + position + '%)').text(controlVal); 
      });
      
      function handleErrors(response) {
        if (!response.ok) alert(response.statusText);
        return response;
      }

      function controlRequest (el) {
        // send control request to server as it occurs
        let value = (el.type == 'checkbox') ? (el.checked ? 1 : 0) : el.value
        const query = `${baseHost}/control?${el.id}=${value}`
        const encoded = encodeURI(query);
        fetch(encoded)
          .then(handleErrors)
          .then(response => {
            console.log(`request to ${query} finished, status: ${response.status}`)
        })
      }
      
      $(document).on('click','button', function(){
        // send out update request supplying value of pressed button
        if (this.classList.contains('update-action')) sendUpdates(this.value);
        if (this.classList.contains('control-action')) controlRequest(this);
        if (this.classList.contains('download-action')) window.location.href='/control?download=1';  
      });

      document.addEventListener('DOMContentLoaded', function (event) {
      
      // send out control request after change to any input or select tag containing class="control-action"
        document
          .querySelectorAll('.control-action')
          .forEach(el => {
            el.onchange = () => controlRequest(el)
          })
          
        // send out update request after change to any input or select tag containing class="update-action"
        document
          .querySelectorAll('.update-action')
          .forEach(el => {
            el.onchange = () => sendUpdates(el.value)
          })
      });