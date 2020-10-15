todo:  
  - [x] machine info  
  - [x] status, position, program polling  
  - [ ] better header parsing  


Connect kafka
  - Retry
Connect machine
  - Send success
  - Send failure, retry
Poll values
  - Count, periodically stdout

Goals
  - Real-time
    - Position
    - Load
    - Program
    - Line number
  - When relevant
    - Entire program contents



Determine time remaining based on previous runs
+----------------------------------------------------+
|                                                    |
|  %                                                 |
|  O2094(M12093-001 45 DEGREE SUB LEFT OF ZERO.MCV)  |
|  G91                                               | 
|  G00Z-.0625                                        |                                                            
|  G00X-.0625                                        |                                                            
|  G01Y3.F42.                                        |                                                            
|  G00Z.5                                            |                                                        
|----------------------------------------------------|
|> G00Y-3.                                           |                                                        
|----------------------------------------------------|
|  G00Z-.5                                           |                                                        
|  G90                                               |                                                    
|  M99                                               |                                                    
|  %                                                 |
|                                                    |
+----------------------------------------------------+
