var initLastStartup = JSON.parse(dataLastStartup)
var initPinState    = JSON.parse(dataPinState)
var initSchedule    = JSON.parse(dataSchedule)
var urls            = JSON.parse(dataURLS)

const setpinURL           = urls.urls["setpin"]
const setpintimedURL      = urls.urls["setpintimed"]
const readpinURL          = urls.urls["readpin"]
const readpinsocketURL    = "ws://" + window.location.hostname + urls.urls["readpinsocket"]
const setlowURL           = urls.urls["setlow"]
const sethighURL          = urls.urls["sethigh"]
const addScheduleURL      = urls.urls["addschedule"]
const deleteScheduleURL   = urls.urls["deleteschedule"]
const getScheduleURL      = urls.urls["getschedule"]
const espRestartURL       = urls.urls["esprestart"]
const getLastStartupURL   = urls.urls["getlaststartup"]
const startWMURL          = urls.urls["startwm"]
const wlanResetURL        = urls.urls["wlanreset"]

const global = Vue.observable({
    allDisable: false,
    lastStartup: initLastStartup,
    timeNow: Date.now()
})

var WSDisconnected = false
var lastWSNotify = 0
var ws

function socketOnMessage(event){
    if(event.data == "ONLINE"){
        lastWSNotify = global.timeNow
    }else{
        try{
            var data = JSON.parse(event.data)
            
            if(data.success == true){
                window.pinControl.pinStateJSON = data
                window.pinControl.updatingState = false
                console.log("pin update success")
            }
        }catch(error){
            console.log("pin update error")
        }
    }
}

function startWebsocket() {
    ws = new WebSocket(readpinsocketURL)

    ws.onclose = function(){
        WSDisconnected = true

        console.log("websocket disconnected")

        window.options.WSDisconnected()
    }
    ws.onmessage = socketOnMessage
    ws.onopen = function(){
        WSDisconnected = false
        lastWSNotify = global.timeNow

        console.log("websocket connected")
    }
}

function resetDisable(){
    global.allDisable = false
}

function periodic(){
    global.timeNow = Date.now()

    if(global.timeNow - lastWSNotify >= 10000 && !WSDisconnected){
        WSDisconnected = true

        console.log("websocket disconnected")

        window.options.WSDisconnected()
    }
}

setInterval(periodic, 1000)
startWebsocket()

window.pinControl = new Vue({
    components: {
        'pin-panel': Vue.component('pin-panel',{
            components:{
                'pin-component': Vue.component('pin-component', {
                    computed: {
                        pinToggle: function (){
                            return this.$root.pinStateJSON.state[this.pin] == "1" ? true : false 
                        },
                        validDuration: function(){
                            if(this.duration == null || this.duration.trim() == ''){
                                return null
                            }else{
                                return this.duration >= 1 && this.duration <= 604800000
                            }
                        }
                    },
                    data: function(){
                        return{
                            duration: null
                        }
                    },
                    methods: {
                        pinClick: function(){
                            global.allDisable = true

                            const params = new URLSearchParams()
                            params.append("pin", this.pin)
                            params.append("state", !this.$root.pinStateJSON.state[this.pin] ? 1 : 0)

                            axios.post(setpinURL, params, {
                                timeout: 5000
                            })
                            .then(function (response) {
                                var data = response.data

                                if(data.success == true){
                                    window.pinControl.updatingState = true
                                }else if(data.success == false){
                                    window.options.modalOK("Set Pin Failed (" + data.message + ")", {
                                        title: "Error",
                                        centered: true,
                                        okVariant: "danger"
                                    })
                                }
                            })
                            .catch(function(error){
                                window.options.modalOK("Set Pin Error (" + error + ")", {
                                    title: "Error",
                                    centered: true,
                                    okVariant: "danger"
                                })
                            })
                            .finally(function(){
                                global.allDisable = false
                            })
                        },
                        pinTimedClick: function(){
                            global.allDisable = true
                            
                            const params = new URLSearchParams()
                            params.append("pin", this.pin)
                            params.append("duration", this.duration)

                            this.duration = null

                            axios.post(setpintimedURL, params, {
                                timeout: 5000
                            })
                            .then(function (response) {
                                var data = response.data

                                if(data.success == true){
                                    window.pinControl.updatingState = true
                                }else if(data.success == false){
                                    window.options.modalOK("Set Pin Failed (" + data.message + ")", {
                                        title: "Error",
                                        centered: true,
                                        okVariant: "danger"
                                    })
                                }
                            })
                            .catch(function(error){
                                window.options.modalOK("Set Pin Error (" + error + ")", {
                                    title: "Error",
                                    centered: true,
                                    okVariant: "danger"
                                })
                            })
                            .finally(function(){
                                global.allDisable = false
                            })
                        }
                    },
                    props: ['pin'],
                    template:
                    '<b-container fluid>' +
                        '<b-row>' +
                            '<b-col sm md="auto" class="text-center">' +
                                'Pin {{this.pin}}'+
                            '</b-col>' +
                            '<b-col sm></b-col>'+
                            '<b-col sm="4" class="text-center ml-auto">' +
                                '<b-form v-on:submit.stop.prevent="handleSubmit">' +
                                    '<b-form-input v-model="duration" type="number" min=1 max=604800000 :state="this.validDuration" placeholder="Duration (1-604800000 ms, blank -> no duration)"></b-form-input>' +
                                '</b-form>' + 
                            '</b-col>' +
                            '<b-col sm md="auto" class="text-center">' +
                                '<b-button class="mx-1" :disabled="this.$root.btnDisabled == true || validDuration == null || validDuration == false" variant="success" v-on:click="pinTimedClick">SET TIMED</b-button>' +
                                '<b-button :disabled="this.$root.btnDisabled == true" :variant="pinToggle ? \'success\' : \'danger\' " v-on:click="pinClick">{{ pinToggle ? "ON" : "OFF" }}</b-button>' +
                            '</b-col>' +
                        '</b-row>' +
                    '</b-container>'
                })
            },
            computed: {
                pinList () {
                    return Object.keys(this.$root.pinStateJSON.state)
                }
            },
            methods: {
                highClick: function(){
                    global.allDisable = true

                    axios.post(sethighURL, {
                        timeout: 5000
                    })
                    .then(function (response) {
                        var data = response.data

                        if(data.success == true){
                            window.pinControl.updatingState = true
                        }else if(data.success == false){
                            window.options.modalOK("Set Pin Failed (" + data.message + ")", {
                                title: "Error",
                                centered: true,
                                okVariant: "danger"
                            })
                        }
                    })
                    .catch(function(error){
                        window.options.modalOK("Set Pin Error (" + error + ")", {
                            title: "Error",
                            centered: true,
                            okVariant: "danger"
                        })
                    })
                    .finally(function(){
                        global.allDisable = false
                    })
                },
                lowClick: function(){
                    global.allDisable = true

                    axios.post(setlowURL, {
                        timeout: 5000
                    })
                    .then(function (response) {
                        var data = response.data

                        if(data.success == true){
                            window.pinControl.updatingState = true
                        }else if(data.success == false){
                            window.options.modalOK("Set Pin Failed (" + data.message + ")", {
                                title: "Error",
                                centered: true,
                                okVariant: "danger"
                            })
                        }
                    })
                    .catch(function(error){
                        window.options.modalOK("Set Pin Error (" + error + ")", {
                            title: "Error",
                            centered: true,
                            okVariant: "danger"
                        })
                    })
                    .finally(function(){
                        global.allDisable = false
                    })
                }
            },
            template:
            '<div>' +
                '<b-card>'+
                    '<b-container fluid>' +
                        '<b-row>'+
                            '<b-col sm md="auto" class="text-center">' +
                                '<b-button :disabled="this.$root.btnDisabled == true" variant="success" v-on:click="highClick">SET ALL ON</b-button>' +
                            '</b-col>' +
                            '<b-col></b-col>' +
                            '<b-col sm md="auto" class="text-center">' +
                                '<b-button :disabled="this.$root.btnDisabled == true" variant="danger" v-on:click="lowClick">SET ALL OFF</b-button>' +
                            '</b-col>' +
                        '</b-row>'+
                    '</b-container>' +
                '</b-card>' +
                '<div v-for="num in pinList">' + 
                    '<b-card>'+
                        '<pin-component :pin=num></pin-component>'+
                    '</b-card>' +
                '</div>' +
            '</div>'
        })
    },
    computed: {
        btnDisabled: function (){
            return global.allDisable || this.updatingState
        }
    },
    data: {
        pinStateJSON: initPinState,
        updatingState: false
    },
    el: '#pinControl',
    methods:{
        refreshPinState(){
            const params = new URLSearchParams()
            params.append("socketonly", null)

            axios.post(readpinURL, params,{
                timeout: 5000
            })
        }
    }
})

window.scheduler = new Vue({
    components:{
        'add-schedule' : Vue.component('add-schedule',{
            computed:{
                dataValid: function(){
                    if(this.formData.type == "ot" || this.formData.type == "r"){
                        if(this.formData.pin != null && Object.keys(window.pinControl.pinStateJSON.state).includes(this.formData.pin)){
                            if(this.formData.type == "ot"){
                                if(this.formData.oneTime.startDate != null &&  this.formData.oneTime.endDate != null){
                                    if(this.formData.oneTime.startTime != '' && this.formData.oneTime.endTime != ''){
                                        if(this.startDateValid && this.startTimeValid && this.endDateValid && this.endTimeValid){
                                            var stsplit = this.formData.oneTime.startTime.split(":")
                                            var etsplit = this.formData.oneTime.endTime.split(":")

                                            this.formData.oneTime.startTimestamp = new Date(this.formData.oneTime.startDate)
                                            this.formData.oneTime.startTimestamp.setHours(parseInt(stsplit[0]))
                                            this.formData.oneTime.startTimestamp.setMinutes(parseInt(stsplit[1]))
                                            this.formData.oneTime.startTimestamp.setSeconds(0)

                                            this.formData.oneTime.endTimestamp = new Date(this.formData.oneTime.endDate)
                                            this.formData.oneTime.endTimestamp.setHours(parseInt(etsplit[0]))
                                            this.formData.oneTime.endTimestamp.setMinutes(parseInt(etsplit[1]))
                                            this.formData.oneTime.endTimestamp.setSeconds(0)

                                            return true
                                        }
                                    }
                                }
                            }

                            if(this.formData.type == "r"){
                                if(this.formData.repeating.duration >= 1 && this.formData.repeating.duration <= 86400 && this.formData.repeating.time != ""){
                                    var splitted = this.formData.repeating.time.split(":")

                                    this.formData.repeating.hour = parseInt(splitted[0])
                                    this.formData.repeating.minute = parseInt(splitted[1])

                                    return true
                                }
                            }
                        }
                    }

                    return false
                },
                selectedType: function(){
                    this.formData.pin = null
                    this.formData.oneTime.startDate = null
                    this.formData.oneTime.startTime = ""
                    this.formData.oneTime.endDate= null
                    this.formData.oneTime.endTime = ""
                    this.formData.oneTime.startTimestamp = null
                    this.formData.oneTime.endTimestamp = null
                    this.formData.repeating.duration = null
                    this.formData.repeating.time = ""
                    this.formData.repeating.hour = null
                    this.formData.repeating.minute = null
                    return this.formData.type
                },
                startDateValid: function(){
                    return this.formData.oneTime.startDate != null
                },
                endDateValid: function(){
                    if(this.startDateValid && this.formData.oneTime.endDate != null){
                        if(new Date(this.formData.oneTime.startDate) <= new Date(this.formData.oneTime.endDate) && new Date(this.formData.oneTime.endDate) <= new Date(new Date(this.formData.oneTime.startDate).getTime() + 604800000)){
                            return true
                        }
                    }
                    
                    return false
                },
                startTimeValid: function(){
                    if(this.startDateValid && this.formData.oneTime.startTime != ''){
                        var stsplit = this.formData.oneTime.startTime.split(":")
                        var tempST = new Date(this.formData.oneTime.startDate)

                        tempST.setHours(stsplit[0])
                        tempST.setMinutes(stsplit[1])
                        tempST.setSeconds(0)

                        return tempST > global.timeNow
                    }
                    
                    return false
                },
                endTimeValid: function(){
                    if(this.startDateValid && this.endDateValid && this.formData.oneTime.endTime != ''){
                        var stsplit = this.formData.oneTime.startTime.split(":")
                        var etsplit = this.formData.oneTime.endTime.split(":")

                        if(new Date(this.formData.oneTime.endDate).getTime() > new Date(this.formData.oneTime.startDate).getTime()){
                            var startDate = new Date(this.formData.oneTime.startDate)
                            var endDate = new Date(this.formData.oneTime.endDate)

                            startDate.setHours(stsplit[0])
                            startDate.setMinutes(stsplit[1])
                            startDate.setSeconds(0)

                            endDate.setHours(parseInt(etsplit[0]))
                            endDate.setMinutes(parseInt(etsplit[1]))
                            endDate.setSeconds(0)

                            return endDate.getTime() <= new Date(startDate.getTime() + 604800000).getTime()
                        }else if(new Date(this.formData.oneTime.endDate).getTime() == new Date(this.formData.oneTime.startDate).getTime()){
                            if(this.startTimeValid){
                                return (parseInt(etsplit[0])*60 + parseInt(etsplit[1])) > (parseInt(stsplit[0])*60 + parseInt(stsplit[1]))
                            }
        
                            return false
                        }
                    }
                    
                    return false
                },
                minDate: function(){
                    return this.formData.oneTime.startDate != null ? this.formData.oneTime.startDate : new Date()
                },
                maxDate: function(){
                    return this.formData.oneTime.startDate != null ? new Date(new Date(this.formData.oneTime.startDate).getTime() + 604800000) : null
                }
            },
            created: function(){
                this.generatePO()
            },
            data: function(){
                return{
                    formOptions:{
                        typeOptions: [
                            { text: 'One Time Schedule', value: "ot" },
                            { text: 'Repeating Schedule', value: "r" }
                        ],
                        pinOptions: [{text: "-Select Pin-", value: null}]
                    },
                    formData: {
                        type: null,
                        pin : null,
                        oneTime: {
                            startDate: null,
                            startTime: "",
                            endDate: null,
                            endTime: "",
                            startTimestamp: null,
                            endTimestamp: null
                        },
                        repeating: {
                            duration: null,
                            time: "",
                            hour: null, 
                            minute: null,
                        }
                    }
                }
            },
            methods:{
                reset: function(){
                    this.formData.type = null
                    this.formData.pin = null
                    this.formData.oneTime.startDate = null
                    this.formData.oneTime.startTime = ""
                    this.formData.oneTime.endDate= null
                    this.formData.oneTime.endTime = ""
                    this.formData.oneTime.startTimestamp = null
                    this.formData.oneTime.endTimestamp = null
                    this.formData.repeating.duration = null
                    this.formData.repeating.time = ""
                    this.formData.repeating.hour = null
                    this.formData.repeating.minute = null
                },
                generatePO: function(){
                    var pins = Object.keys(window.pinControl.pinStateJSON.state)
                    for(var n in pins){
                        this.formOptions.pinOptions.push({text: pins[n], value: pins[n]})
                    }
                },
                handleOkClick(){
                    this.handleSubmit()
                },
                handleSubmit(){
                    global.allDisable = true
                    const params = new URLSearchParams()

                    params.append("type", this.formData.type == "ot" ? "onetime" : (this.formData.type == "r" ? "repeating" : ""))
                    params.append("pin", this.formData.pin)
                    
                    if(this.formData.type == "ot"){
                        params.append("startTimestamp", this.formData.oneTime.startTimestamp.getTime() / 1000)
                        params.append("endTimestamp", this.formData.oneTime.endTimestamp.getTime() / 1000)
                    }else if(this.formData.type == "r"){
                        params.append("hour", this.formData.repeating.hour)
                        params.append("minute", this.formData.repeating.minute)
                        params.append("duration", this.formData.repeating.duration)
                    }
                    
                    axios.post(addScheduleURL, params,{
                        timeout: 5000
                    })
                    .then(function (response) {
                        var data = response.data

                        if(data.success == true){
                            window.options.modalOK("Add Schedule Success", {
                                title: "Success",
                                centered: true,
                                okVariant: "success"
                            })
                            window.scheduler.refreshSchedule()
                        }else if(data.success == false){
                            window.options.modalOK("Add Schedule Failed (" + data.message + ")", {
                                title: "Error",
                                centered: true,
                                okVariant: "danger"
                            })
                        }
                    })
                    .catch(function(error){
                        window.options.modalOK("Add Schedule Error (" + error + ")", {
                            title: "Error",
                            centered: true,
                            okVariant: "danger"
                        })
                    })
                    .finally(function(){
                        global.allDisable = false
                    })
                }
            },
            template:
            '<div>' +
                '<span v-if="this.$root.schedule.available">' + 
                    '<b-button :disabled="this.$root.btnDisabled == true" v-on:click="$bvModal.show(\'addsched\')">Add Schedule</b-button>' +
                '</span>' +
                '<span v-else>Schedule not available until internet connection available</span>' +
                '<b-modal id="addsched" title="Add Schedule" v-on:hidden="this.reset" centered hide-header-close :okDisabled="dataValid == false" v-on:ok="handleOkClick">' + 
                    '<form v-on:submit.stop.prevent="handleSubmit">' +
                        '<b-form-group label="Schedule Type">' + 
                            '<b-form-radio-group v-model="formData.type" :options="formOptions.typeOptions"></b-form-radio-group>' +
                        '</b-form-group>' +
                        '<div v-if="this.selectedType != null">'+
                            '<hr>' +
                            '<label for="pinPicker">Pin</label><br>' +
                            '<b-form-select class="w-100" id="pinPicker" size="lg" v-model="formData.pin" :options="formOptions.pinOptions" :state="this.formData.pin != null"></b-form-select>' +
                        '</div>' +
                        '<div v-if="this.selectedType == \'ot\'">' +
                            '<b-form-group label="Start Time">'+
                                '<b-form-datepicker value-as-date v-model="formData.oneTime.startDate" locale="id" :state="this.startDateValid" :min="new Date()" placeholder="Start Date" today-button reset-button close-button></b-form-datepicker>' +
                                '<b-form-timepicker v-model="formData.oneTime.startTime" locale="id" :state="this.startTimeValid" placeholder="Start Time" now-button reset-button></b-form-timepicker>' +
                            '</b-form-group>' +
                            '<b-form-group label="End Time">'+
                                '<b-form-datepicker value-as-date v-model="formData.oneTime.endDate" locale="id" :state="this.endDateValid" :min="this.minDate" :max="this.maxDate" placeholder="End Date" today-button reset-button close-button></b-form-datepicker>' +
                                '<b-form-timepicker v-model="formData.oneTime.endTime" locale="id" :state="this.endTimeValid" placeholder="End Time" now-button reset-button></b-form-timepicker>' +
                            '</b-form-group>' +
                        '</div>' +
                        '<div v-if="this.selectedType == \'r\'">' +
                            '<label for="rTimePicker">Time</label>' +
                            '<b-form-timepicker id="rTimePicker" v-model="formData.repeating.time" locale="id" :state="this.formData.repeating.time != \'\'" placeholder="Repeating Every Day" now-button reset-button></b-form-timepicker>' +
                            '<label for="durationPicker">Duration (in seconds)</label>' +
                            '<b-form-input id="durationPicker" v-model="formData.repeating.duration" type="number" min=1 max=86400 :state="this.formData.repeating.duration >= 1 && this.formData.repeating.duration <= 86400" placeholder="Duration"></b-form-input>' +
                        '</div>' +
                    '</form>' + 
                '</b-modal>' +
            '</div>'
        }),
        'del-schedule' : Vue.component('del-schedule',{
            methods:{
                delmodal: function(modalMsg, schedid){ 
                    this.$bvModal.msgBoxConfirm(modalMsg, {
                        title: 'Delete Schedule',
                        okVariant: 'danger',
                        centered: true
                    }).
                    then(function(value){
                        if(value == true){
                            const params = new URLSearchParams()
                            params.append("id", schedid)

                            axios.post(deleteScheduleURL, params,{
                                timeout: 5000
                            })
                            .then(function (response) {
                                var data = response.data
        
                                if(data.success == true){
                                    window.options.modalOK("Delete Schedule Success", {
                                        title: "Success",
                                        centered: true,
                                        okVariant: "success"
                                    })
                                    window.scheduler.refreshSchedule()
                                }else if(data.success == false){
                                    window.options.modalOK("Delete Schedule Failed (" + data.message + ")", {
                                        title: "Error",
                                        centered: true,
                                        okVariant: "danger"
                                    })
                                }
                            })
                            .catch(function(error){
                                window.options.modalOK("Delete Schedule Error (" + error + ")", {
                                    title: "Error",
                                    centered: true,
                                    okVariant: "danger"
                                })
                            })
                        }
                    }).catch(function(error){
                        console.log(error)
                    })
                },
                delsched: function(){
                    this.delmodal("Delete Schedule (ID : " + this.sid + ") ?", this.sid)
                }
            },
            props: ['sid'],
            template:
            '<b-button :disabled="this.$root.btnDisabled == true" v-on:click="delsched">Delete Schedule</b-button>'
        }),
        'ot-schedule' : Vue.component('ot-schedule',{
            components:{
                'ot-schedule-panel': Vue.component('ot-schedule-panel',{
                    computed:{
                        start: function(){
                            return new Date(this.schedule.startTimestamp*1000).toLocaleString("id")
                        },
                        end: function(){
                            return new Date(this.schedule.endTimestamp*1000).toLocaleString("id")
                        } 
                    },     
                    props:['schedule'],
                    template:
                    '<b-container fluid>' +
                        '<b-row>' + 
                            '<b-col sm md="auto" class="text-center">' +
                                '<span v-if="this.$root.now/1000 >= schedule.startTimestamp && this.$root.now/1000 < schedule.endTimestamp">' +
                                    '<span class="fw-bold">At {{ start }} to {{ end }} at pin {{schedule.pin}} (Running)</span>' +
                                '</span>' +
                                '<span v-else>' +
                                    '<span>At {{ start }} to {{ end }} at pin {{schedule.pin}}</span>' +
                                '</span>' + 
                            '</b-col>' +
                            '<b-col></b-col>' +
                            '<b-col sm md="auto" class="text-center">' +
                                '<del-schedule :sid="schedule.id"></del-schedule>' +
                            '</b-col>' +
                        '</b-row>' + 
                    '</b-container>'
                })
            },
            computed:{
                otSchedule: function(){
                    return this.$root.schedule.onetime
                }
            },
            created: function(){
                setInterval(this.scheduleEnd, 1000)
            },
            methods:{
                scheduleEnd: function(){
                    for(n in this.otSchedule){
                        if(Date.now()/1000 >= this.otSchedule[n].endTimestamp && !this.$root.refreshing){
                            this.$root.refreshSchedule()
                        }
                    }
                }
            },
            template: 
            '<div>' +  
                '<div v-for="schedule in this.otSchedule">' +
                    '<b-card>' +
                        '<ot-schedule-panel :schedule="schedule"></ot-schedule-panel>' +
                    '</b-card>'+ 
                '</div>' + 
            '</div>'
        }),
        'r-schedule' : Vue.component('r-schedule',{
            components:{
                'r-schedule-panel': Vue.component('r-schedule-panel',{     
                    methods:{
                        startTimestamp: function(schedule){
                            return new Date(this.$root.year, this.$root.month, this.$root.date, schedule.hour, schedule.minute, 0).getTime()
                        },
                        endTimestamp: function(schedule){
                            return new Date(new Date(this.$root.year, this.$root.month, this.$root.date, schedule.hour, schedule.minute, 0).getTime() + schedule.duration*1000).getTime()
                        }
                    },
                    props:['schedule'],
                    template:
                    '<b-container fluid>' +
                        '<b-row>' + 
                            '<b-col sm md="auto" class="text-center">' +
                                '<span v-if="this.$root.now > startTimestamp(schedule) && this.$root.now < endTimestamp(schedule)">' +
                                    '<span class="fw-bold">Every {{schedule.hour < 10 ? "0" + schedule.hour : schedule.hour}}:{{schedule.minute < 10 ? "0" + schedule.minute : schedule.minute}} on pin {{schedule.pin}} for {{schedule.duration}} seconds (Running)</span>' +
                                '</span>' +
                                '<span v-else>' +
                                    '<span>Every {{schedule.hour < 10 ? "0" + schedule.hour : schedule.hour}}:{{schedule.minute < 10 ? "0" + schedule.minute : schedule.minute}} on pin {{schedule.pin}} for {{schedule.duration}} seconds</span>' + 
                                '</span>' + 
                            '</b-col>' +
                            '<b-col></b-col>' +
                            '<b-col sm md="auto" class="text-center">' +
                                '<del-schedule :sid="schedule.id"></del-schedule>' +
                            '</b-col>' +
                        '</b-row>' + 
                    '</b-container>'
                })
            },
            computed:{
                rSchedule: function(){
                    return this.$root.schedule.repeating
                }
            },
            template:
            '<div>' + 
                '<div v-for="schedule in this.rSchedule">' +
                    '<b-card>' +
                        '<r-schedule-panel :schedule="schedule"></r-schedule-panel>' +
                    '</b-card>'+ 
                '</div>' + 
            '</div>'
        })
    },
    computed:{
        btnDisabled: function (){
            return global.allDisable || !this.$root.schedule.available || this.$root.refreshing
        },
        year: function(){
            return new Date().getFullYear()
        },
        month: function(){
            return new Date().getMonth()
        },
        date: function(){
            return new Date().getDate()
        },
        now: function(){
            return global.timeNow
        }
    },
    data:{
        refreshing: false,
        schedule: initSchedule
    },
    el: '#scheduler',
    methods:{
        refreshSchedule: function(){
            this.$root.refreshing = true
            
            axios.post(getScheduleURL,{
                timeout: 5000
            })
            .then(function (response) {
                try{
                    window.scheduler.schedule = JSON.parse(JSON.stringify(response.data))
                    console.log("refresh schedule success")
                }catch(e){
                    console.log(e)
                }
            })
            .catch(function(error){
                window.options.modalOK("Error updating schedule (" + error + ")", {
                    title: "Error",
                    centered: true,
                    okVariant: "danger"
                })
            })
            .finally(function(){
                window.scheduler.refreshing = false
            })
        }
    }
})

window.options = new Vue({
    components:{
        'last-startup': Vue.component('last-startup',{
            template:
            '<b-card>' +
                '<b-button pill class="w-100" variant="outline-success" v-b-modal="\'lastStartupModal\'">Get Last ESP Module Startup</b-button>' +
                '<b-modal id="lastStartupModal" title="Last Startup" :ok-variant="this.$root.lastStartupVariant" ok-title="Close" v-on:show="this.$root.refreshLastStartup" centered hide-header-close ok-only>{{this.$root.lastStartup}}</b-modal>' +
            '</b-card>'
        }),
        'restart-esp': Vue.component('restart-esp',{
            template:
            '<b-card>' +
                '<b-button pill class="w-100" variant="outline-danger" v-b-modal="\'restartESPModal\'">Restart ESP Module</b-button>' +
                '<b-modal id="restartESPModal" title="Restart ESP Module" ok-variant="danger" ok-title="Yes" cancel-title="No" centered hide-header-close no-close-on-backdrop no-close-on-esc v-on:ok="this.$root.restartESP">Are you sure to restart ESP module?</b-modal>' +
            '</b-card>'
        }),
        'reset-wlan': Vue.component('reset-wlan',{
            template:
            '<b-card>' +
                '<b-button pill class="w-100" variant="outline-danger" v-b-modal="\'resetWLANModal\'">Reset ESP WLAN Settings</b-button>' +
                '<b-modal id="resetWLANModal" title="Reset WLAN Config" ok-variant="danger" ok-title="Yes" cancel-title="No" centered hide-header-close no-close-on-backdrop no-close-on-esc v-on:ok="this.$root.resetWLAN">Are you sure to reset ESP WLAN config?</b-modal>' +
            '</b-card>'
        }),
        'logout': Vue.component('logout',{
            template:
            '<b-card>' +
                '<b-button pill class="w-100" variant="outline-danger" v-on:click="this.$root.logout">Logout</b-button>' +
            '</b-card>'
        }),
    },
    computed:{
        lastStartup: function(){
            var message

            if(global.lastStartup.success){
                var uptime = global.timeNow / 1000 - new Date(global.lastStartup.message).getTime() / 1000

                var day = uptime / 86400
                uptime %= 86400

                var hour = uptime / 3600
                uptime %= 3600

                var minute = uptime / 60
                uptime %= 60

                message = global.lastStartup.message + " (" + Math.floor(day) + " days " + Math.floor(hour) + " hours " + Math.floor(minute) + " minutes " + Math.floor(uptime) + " seconds since last startup)"
            }else{
                message = "NTP not synced. Please check module internet connection"
            }

            return message
        },
        lastStartupVariant: function(){
            return global.lastStartup.success ? "success" : "danger"
        }
    },
    el: '#options',
    methods: {
        modalOK: function(message, config){
            this.$bvModal.msgBoxOk(message, config)
        },
        logout: function (){
            axios.get("index.html",{
                withCredentials: true,
                auth: {
                    username: 'log',
                    password: 'out'
                }
            })
            .catch(
                window.location.replace("logout.html")
            )
        },
        refreshLastStartup: function(){
            if(!global.lastStartup.success){
                axios.get(getLastStartupURL,{
                    timeout: 5000
                })
                .then(function(response){
                    if(response.data.success == true){
                        global.lastStartup.success = true
                        global.lastStartup.message = response.data.message
                        console.log("last startup refresh success")
                    }else{
                        console.log("last startup refresh no change")
                    }
                })
                .catch(function(error){
                    console.log("last startup refresh fail")
                })
            }
        },
        resetWLAN: function(){
            axios.get(wlanResetURL,{
                timeout: 5000
            })
            .then(function(response){
                if(response.data.success == true){
                    this.window.options.modalOK(response.data.message, {
                        title: 'Reset WLAN Config',
                        centered: true,
                        okVariant: 'success'
                    })
                }else{
                    this.window.options.modalOK("Reset WLAN Config Failed", {
                        title: 'Reset WLAN Config',
                        centered: true,
                        okVariant: 'danger'
                    })
                }
            })
            .catch(function(error){
                this.window.options.modalOK("Reset WLAN Config Failed (" + error + ")", {
                    title: 'Reset WLAN Config',
                    centered: true,
                    okVariant: 'danger'
                })
            })
        },
        restartESP: function(){
            axios.get(espRestartURL,{
                timeout: 5000
            })
            .then(function(response){
                if(response.data.success == true){
                    this.window.options.modalOK(response.data.message, {
                        title: 'Restart ESP Module',
                        centered: true,
                        okVariant: 'success'
                    })
                }else{
                    this.window.options.modalOK("Restart ESP Module Failed", {
                        title: 'Restart ESP Module',
                        centered: true,
                        okVariant: 'danger'
                    })
                }
            })
            .catch(function(error){
                this.window.options.modalOK("Restart ESP Module Failed (" + error + ")", {
                    title: 'Restart ESP Module',
                    centered: true,
                    okVariant: 'danger'
                })
            })
        },
        WSDisconnected: function(){
            this.$bvModal.msgBoxOk("Server not connected. Try to reload the page", {
                title: 'Error',
                centered: true,
                okVariant: 'danger'
            })
            .then(function(value) {
                window.location.reload()
            })
            .catch(function(error) {
                window.location.reload()
            })
        }
    }
})