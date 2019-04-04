/*
	 * Takes a 3 or 6-digit hex color code, and an optional 0-255 numeric alpha value
	 */
	function rgbToHex(r, g, b) {
		return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
	}
	function hexToRGB(hex, alpha) {
	  if (typeof hex !== 'string' || hex[0] !== '#') {
		console.log ("Not a hex color:");
		console.log(hex);
		
		return null; // or return 'transparent'
		}
	  const stringValues = (hex.length === 4)
			? [hex.slice(1, 2), hex.slice(2, 3), hex.slice(3, 4)].map(n => `${n}${n}`)
			: [hex.slice(1, 3), hex.slice(3, 5), hex.slice(5, 7)];
	  const intValues = stringValues.map(n => parseInt(n, 16));
	  return {r:intValues[0],g:intValues[1],b:intValues[2]};
	}
	
	function gradientStateToJSON(gradientvar){
		var safetext = gradientvar.getSafeValue().split('%');
		var handlearray = [];
		for (var i = 0; i< safetext.length -1; i++){
			var rgbp = safetext[i].slice(safetext[i].lastIndexOf('(')+1).replace(')',',').split(',');
			rgbp = rgbp.map(parseFloat);
			handlearray.push({pos:rgbp[3], r:rgbp[0],g:rgbp[1],b:rgbp[2]});
		}
		//console.log(JSON.stringify(handlearray)); //[{"pos":0,"r":0,"g":0,"b":0},{"pos":28.6667,"r":255,"g":255,"b":0},{"pos":44.3333,"r":255,"g":0,"b":0},{"pos":100,"r":255,"g":255,"b":255}]
		return handlearray;
	}

  var gradients = {active:1,grad1 : "None", grad1speed : 0,grad2 : "None", grad2speed :0, grad3 : "None",grad3speed: 0, grad4 : "None", grad4speed:0};
  
  //----------------Fetch JSON defaults from server
	/*let url = 'defaults.json';

	fetch(url)
	.then(res => res.json())
	.then((out) => {
		console.log('Checkout this JSON! ', out);
		gradients = JSON.parse(JSON.stringify(out));
	})
	
	.catch(err => { throw err });*/

	

  function sendGradientsJSON(jsonstr){
	//gradientStateToJSON(gp1);
	/*
	gradients.grad1 = gp1.getSafeValue(); //linear-gradient(to right, rgb(0, 0, 0) 0%, rgb(133, 0, 0) 22%, rgb(255, 0, 0) 50%, rgb(255, 255, 0) 80%, rgb(255, 255, 255) 100%)
	var h = []
	for (var handle in gp1.getHandlers()){ //FUCKED todo
		var hand1 = gp1.getHandler(handle);
		gradients.grad1= {pos:hand1.getPosition(),c: hexToRGB(hand1.getColor())};
		h.push({pos:hand1.getPosition(),c: hexToRGB(hand1.getColor())});
		console.log(hand1.getColor());
		var color = hexToRGB(hand1.getColor());
		console.log(color);
		console.log(hand1.getColor().r);
		console.log(gradients);
	}
	gradients.grad1 = h;*/
	
	var jsonobj = JSON.stringify(gradients);
	const Http = new XMLHttpRequest();
	const url='/setGradients?setGradient='+jsonobj;
	console.log("GETTING URL:"+url);
	return;
	Http.open("GET", url );
	Http.send();
	Http.onreadystatechange=(e)=>{console.log(Http.responseText)};
  }
  
  function createGrapick(id, grapicksettings){
	let mygp = new Grapick({
		el: '#grapick'+id,
		direction: 'to right',
		min: 0,
		max : 100,
		height: '40px',
	});
	mygp.id = id;
	for (let handle in grapicksettings){
		mygp.addHandler(handle.pos, rgbToHex(handle.r,handle.g,handle.b));
	}
	mygp.on('change',function(complete){
		document.body.style.backgroundImage = mygp.getSafeValue();
		if (complete){
			gradients["grad"+mygp.id] = gradientStateToJSON(mygp);
			gradients.active = mygp.id;
			console.log("Change Complete "+mygp.id);
			sendGradientsJSON("noneyet"+mygp.id);
		}
	})
	mygp.emit('change');
	mygp.btn = document.getElementById('btn'+mygp.id);
	mygp.speed = document.getElementById('speed'+mygp.id);
	mygp.activate = function activate(){
		document.body.style.backgroundImage = mygp.getSafeValue();
		console.log("Activate:"+mygp.id);
		gradients.active = 1;
		gradients["grad"+mygp.id+"speed"] = Math.floor(mygp.speed.value*255.0/100.0);
		sendGradientsJSON("noneyet"+mygp.id);
	} 
	mygp.btn.addEventListener('click',mygp.activate);
	mygp.speed.addEventListener('click',mygp.activate);
	return mygp;
  }
  
  
  var gp1 = new Grapick({
	el: '#grapick1',
	direction: 'to right',
	min: 0,
	max: 100,
	height: '40px',
  });
  gradients.grad1.forEach(function (element){
	gp1.addHandler(element.pos, rgbToHex(element.r,element.g,element.b),0)
  });
  <!-- gp1.addHandler(0, '#000000', 0); -->
  <!-- gp1.addHandler(50, '#FF0000', 0); -->
  <!-- gp1.addHandler(80, '#FFFF00', 0); -->
  <!-- gp1.addHandler(100, '#FFFFFF', 0); -->
  gp1.on('change', function(complete) {
	document.body.style.backgroundImage = gp1.getSafeValue();
	if (complete){
	gradients.grad1 = gradientStateToJSON(gp1);
	gradients.active =1 ;
	console.log("complete");
	sendGradientsJSON("noneyet");
	}
  })
  /*gp1.on('select', function(complete) {
	document.body.style.backgroundImage = gp1.getSafeValue();
	console.log("changed");
	console.log(gp1.getSafeValue());
  })*/
  gp1.emit('change');

  const btn1 = document.getElementById('btn1');	  
  const speed1 = document.getElementById('speed1');	  
  function activate1(){
	document.body.style.backgroundImage = gp1.getSafeValue();
	console.log("Activate 1 ");
	gradients.active = 1;
	gradients.grad1speed = Math.floor(speed1.value*255.0/100.0)
	sendGradientsJSON("noneyet");
  };
  btn1.addEventListener('click',activate1);
  speed1.addEventListener('change',activate1);
  
  var gp2 = new Grapick({
	el: '#grapick2',
	direction: 'to right',
	min: 0,
	max: 100,
	height: '40px',
  });
  gp2.addHandler(0, '#000000', 0);
  gp2.addHandler(50, '#00FF00', 0);
  gp2.addHandler(80, '#00FFFF', 0);
  gp2.addHandler(100, '#FFFFFF', 0);
  gp2.on('change', function(complete) {
	document.body.style.backgroundImage = gp2.getSafeValue();
	if (complete){
	gradients.grad2 = gradientStateToJSON(gp2);
	gradients.active =2 ;
	console.log("complete");
	sendGradientsJSON("noneyet");
	}
  })
  /*gp1.on('select', function(complete) {
	document.body.style.backgroundImage = gp1.getSafeValue();
	console.log("changed");
	console.log(gp1.getSafeValue());
  })*/
  gp2.emit('change');

  const btn2 = document.getElementById('btn2');	  
  const speed2 = document.getElementById('speed2');	  
  function activate2(){
	document.body.style.backgroundImage = gp2.getSafeValue();
	console.log("Activate 2 ");
	gradients.active = 2;
	gradients.grad2speed =Math.floor(speed2.value*255.0/100.0)
	sendGradientsJSON("noneyet");
  };
  btn2.addEventListener('click',activate2);
  speed2.addEventListener('change',activate2);
  
  var gp3 = new Grapick({
	el: '#grapick3',
	direction: 'to right',
	min: 0,
	max: 100,
	height: '40px',
  });
  gp3.addHandler(0, '#000000', 0);
  gp3.addHandler(50, '#0000FF', 0);
  gp3.addHandler(80, '#FF00FF', 0);
  gp3.addHandler(100, '#FFFFFF', 0);
  gp3.on('change', function(complete) {
	document.body.style.backgroundImage = gp3.getSafeValue();
	if (complete){
	gradients.grad3 = gradientStateToJSON(gp3);
	gradients.active =3 ;
	console.log("complete");
	sendGradientsJSON("noneyet");
	}
  })
  /*gp1.on('select', function(complete) {
	document.body.style.backgroundImage = gp1.getSafeValue();
	console.log("changed");
	console.log(gp1.getSafeValue());
  })*/
  gp3.emit('change');

  const btn3 = document.getElementById('btn3');	  
  const speed3 = document.getElementById('speed3');	  
  function activate3(){
	document.body.style.backgroundImage = gp3.getSafeValue();
	console.log("Activate 3 ");
	gradients.active = 3;
	gradients.grad3speed =Math.floor(speed3.value*255.0/100.0)
	sendGradientsJSON("noneyet");
  };
  btn3.addEventListener('click',activate3);
  speed3.addEventListener('change',activate3);
  
  var gp4 = new Grapick({
	el: '#grapick4',
	direction: 'to right',
	min: 0,
	max: 100,
	height: '40px',
  });
  gp4.addHandler(0, '#000000', 0);
  gp4.addHandler(50, '#FF00FF', 0);
  gp4.addHandler(80, '#00FF00', 0);
  gp4.addHandler(100, '#FFFFFF', 0);
  gp4.on('change', function(complete) {
	document.body.style.backgroundImage = gp4.getSafeValue();
	if (complete){
	gradients.grad4 = gradientStateToJSON(gp4);
	gradients.active =4 ;
	console.log("complete");
	sendGradientsJSON("noneyet");
	}
  })
  /*gp1.on('select', function(complete) {
	document.body.style.backgroundImage = gp1.getSafeValue();
	console.log("changed");
	console.log(gp1.getSafeValue());
  })*/
  gp4.emit('change');

  const btn4 = document.getElementById('btn4');	  
  const speed4 = document.getElementById('speed4');	  
  function activate4(){
	document.body.style.backgroundImage = gp4.getSafeValue();
	console.log("Activate 4 ");
	gradients.active = 4;
	gradients.grad4speed = Math.floor(speed4.value*255.0/100.0);
	sendGradientsJSON("noneyet");
  };
  btn4.addEventListener('click',activate4);
  speed4.addEventListener('change',activate4);
	var gps = [gp1,gp2,gp3,gp4];

	fetch('defaults.json')
	.then(function(response) {
		return response.text().json;
	})
	.then(function(text) {
	
		console.log('Request successful' + text);
		gradients = text.json();
	}
		/*
		for (int i = 0; i < gps.length; i++){
			var gp = gps[i];
			
			for ( j = gpX.getHandlers().length -1,j>=0;j--){
				gp.getHandler(j).remove();
			}
			for (var j = 0; j< gradients.grad1 in gradients.grad1:
			gpX.addHandler((gradient.pos*100)/255, rgbToHex(gradient.r,gradient.b,gradient.g),0);
		}*/

	)
	.catch(function(error) {
	log('Request failed', error)
	});

